// Sonnenschein Decky plugin — Steam library helpers.
//
// Non-Steam shortcut handling (AddShortcut/SetShortcutName/RunGame via
// overview.gameid/hiding via collectionStore.SetAppsAsHidden) follows the
// pattern proven by MoonDeck (GPL-3, https://github.com/FrogTheFrog/moondeck),
// see its launchApp.ts / getGameId.ts.

import { callable } from "@decky/api";

export interface HostInfo {
  name: string;
  uuid: string;
  address: string;
  port: number;
}

export interface HostApp {
  id: string;
  title: string;
}

export interface LibraryApp {
  appid: number;
  display_name: string;
}

// "<hostUuid>/<appId>" -> steam appId, plus hidden runtime entries below.
export type SyncState = Record<string, number>;

const LEGACY_STREAM_SHORTCUT_STATE_KEY = "_streamShortcut";
const STREAM_SHORTCUT_STATE_PREFIX = "_nativeStream/";

// Non-Steam shortcuts report this app_type — excluded from library matching
// so our own auto-synced/stream shortcuts never match themselves.
const STEAM_GAME_APP_TYPE = 1;

declare const SteamClient: any;
declare const appStore: any;
declare const collectionStore: any;

const getState = callable<[], SyncState>("get_state");
const setState = callable<[SyncState], boolean>("set_state");
const getLaunchOptions = callable<[string, string], string>("get_launch_options");

let cachedRunnerPath = "";

// Set once at plugin load and again on every panel refresh (see index.tsx) —
// the game-details-page button (gamepage.tsx) has no React state of its own
// to read this from.
export function setRunnerPath(path: string): void {
  cachedRunnerPath = path;
}

// Reverse index for the game-details-page patch: normalized host game title
// -> which host/app it came from. Populated from get_apps() results, kept
// separate from the Steam-library lookup below (different direction).
export const hostGameIndex = new Map<string, { host: HostInfo; app: HostApp }>();
const hostSteamGameIndex = new Map<number, { host: HostInfo; app: HostApp }>();
let hostGameIndexRevision = 0;
const hostGameIndexListeners = new Set<() => void>();
const hostSteamAppIds = new Set<string>();

// React game-page patches may render before the asynchronous host catalogue
// arrives. A plain Map mutation does not trigger a rerender, so expose the
// smallest possible external-store interface around the existing index.
export function subscribeHostGameIndex(listener: () => void): () => void {
  hostGameIndexListeners.add(listener);
  return () => hostGameIndexListeners.delete(listener);
}

export function getHostGameIndexRevision(): number {
  return hostGameIndexRevision;
}

export function updateHostGameIndex(host: HostInfo, apps: HostApp[]): void {
  hostGameIndex.clear();
  hostSteamGameIndex.clear();
  hostSteamAppIds.clear();
  for (const app of apps) {
    const key = normalizeTitle(app.title);
    if (key) {
      hostGameIndex.set(key, { host, app });
    }
    const libraryApp = findLibraryAppByTitle(app.title);
    if (libraryApp) {
      hostSteamAppIds.add(String(libraryApp.appid));
      hostSteamGameIndex.set(libraryApp.appid, { host, app });
    }
  }
  hostGameIndexRevision++;
  for (const listener of hostGameIndexListeners) {
    listener();
  }
}

export function getHostGameForSteamApp(
  appId: number,
  appName: string
): { host: HostInfo; app: HostApp } | undefined {
  return hostSteamGameIndex.get(appId) ?? hostGameIndex.get(normalizeTitle(appName));
}

export function isHostStreamableAppId(appId: string): boolean {
  if (hostSteamAppIds.has(appId)) {
    return true;
  }
  // The host catalogue can arrive before Steam has populated appStore. The
  // capsule observer retries, so resolve the title lazily once Steam is ready.
  try {
    const overview = appStore?.GetAppOverviewByAppID?.(Number(appId));
    const title = overview?.display_name;
    const entry = typeof title === "string" ? hostGameIndex.get(normalizeTitle(title)) : undefined;
    if (entry) {
      hostSteamAppIds.add(appId);
      hostSteamGameIndex.set(Number(appId), entry);
      hostGameIndexRevision++;
      for (const listener of hostGameIndexListeners) {
        listener();
      }
      return true;
    }
  } catch (_) {
    // Steam store not ready yet; the next observer scan retries.
  }
  return false;
}

// lowercase, trim, collapse whitespace, drop ™/®/©, drop trailing symbols —
// good enough for matching host game titles against Steam library titles.
export function normalizeTitle(title: string): string {
  return title
    .replace(/[™®©]/g, "")
    .toLowerCase()
    .trim()
    .replace(/\s+/g, " ")
    .replace(/[^a-z0-9)\]]+$/g, "")
    .trim();
}

// Forward lookup: does this host game title already exist as a real Steam
// library entry (installed or just visible in the account)?
export function findLibraryAppByTitle(title: string): LibraryApp | null {
  const target = normalizeTitle(title);
  if (!target) {
    return null;
  }
  try {
    const map: Map<number, any> | undefined = appStore?.m_mapApps;
    if (!map) {
      return null;
    }
    for (const app of map.values()) {
      if (app?.app_type !== STEAM_GAME_APP_TYPE) {
        continue;
      }
      if (typeof app.display_name !== "string") {
        continue;
      }
      if (normalizeTitle(app.display_name) === target) {
        return { appid: app.appid, display_name: app.display_name };
      }
    }
  } catch (e) {
    console.error("Sonnenschein: findLibraryAppByTitle failed", e);
  }
  return null;
}

export function shortcutExists(appId: number): boolean {
  try {
    return !!appStore.GetAppOverviewByAppID(appId);
  } catch (_) {
    return false;
  }
}

export function storedShortcutIsUsable(appId: number): boolean {
  // Steam has no proven "all non-Steam shortcuts loaded" signal. Even when
  // m_mapApps already contains native games, shortcuts may still be warming
  // up. Persisted identity therefore wins; bounded launch-time lookup reports
  // a genuinely missing shortcut without ever creating a duplicate.
  return Number.isFinite(appId) && appId > 0;
}

async function waitForShortcutGameId(appId: number, timeoutMs = 6000): Promise<any | null> {
  const deadline = Date.now() + timeoutMs;
  do {
    const overview = appStore?.GetAppOverviewByAppID?.(appId);
    const gameId = overview?.gameid ?? overview?.m_gameid;
    if (gameId) {
      return gameId;
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  } while (Date.now() < deadline);
  return null;
}

// All frontend code that changes the persisted shortcut map uses this queue.
// Decky can refresh the library while a game-page click is creating its
// runtime entry; serializing the complete read/modify/write cycle prevents
// either operation from overwriting the other's freshly written keys.
let stateMutationQueue: Promise<void> = Promise.resolve();

export function mutateSyncState<T>(
  mutation: (state: SyncState) => Promise<T> | T
): Promise<T> {
  const operation = stateMutationQueue.then(async () => {
    const state = await getState();
    const result = await mutation(state);
    await setState(state);
    return result;
  });
  stateMutationQueue = operation.then(() => undefined, () => undefined);
  return operation;
}

export async function addShortcut(name: string, exe: string): Promise<number | null> {
  try {
    // Newer Steam builds ignore the name argument — set it explicitly after.
    const appId: number = await SteamClient.Apps.AddShortcut(name, exe, "", "");
    if (typeof appId !== "number" || appId <= 0) {
      return null;
    }
    try {
      await SteamClient.Apps.SetShortcutName(appId, name);
    } catch (_) { /* older API sets it via AddShortcut already */ }
    return appId;
  } catch (e) {
    console.error("Sonnenschein: AddShortcut failed", e);
    return null;
  }
}

export function launchGame(steamAppId: number): void {
  try {
    const overview = appStore.GetAppOverviewByAppID(steamAppId);
    const gameId = overview?.gameid ?? overview?.m_gameid;
    if (!gameId) {
      throw new Error("Shortcut nicht gefunden — bitte neu synchronisieren.");
    }
    SteamClient.Apps.RunGame(gameId, "", -1, 100);
  } catch (e) {
    console.error("Sonnenschein: RunGame failed", e);
  }
}

const streamShortcutPromises = new Map<string, Promise<number | null>>();

function streamShortcutKey(host: HostInfo, app: HostApp): string {
  return `${STREAM_SHORTCUT_STATE_PREFIX}${host.uuid}/${app.id}`;
}

// A native library page needs an external process, but Steam cannot attach an
// arbitrary AppImage to an uninstalled Store app safely. Keep exactly one
// hidden runtime shortcut per title instead. Its stable appId gives every game
// its own running state, overlay and Steam Input profile without a visible
// duplicate in the library.
export function ensureStreamShortcut(host: HostInfo, app: HostApp): Promise<number | null> {
  const key = streamShortcutKey(host, app);
  const pending = streamShortcutPromises.get(key);
  if (pending) {
    return pending;
  }

  const operation = mutateSyncState(async (state) => {
    if (!cachedRunnerPath) {
      console.error("Sonnenschein: runner path not loaded yet");
      return null;
    }

    const existingId = state[key];
    if (typeof existingId === "number") {
      // Always trust persisted identity. Steam exposes no reliable distinction
      // between "deleted" and "not published to appStore yet" here.
      if (storedShortcutIsUsable(existingId)) {
        return existingId;
      }
      delete state[key];
    }

    const appId = await addShortcut(app.title, cachedRunnerPath);
    if (appId === null) {
      return null;
    }
    try {
      collectionStore.SetAppsAsHidden([appId], true);
    } catch (e) {
      console.error("Sonnenschein: hiding stream shortcut failed", e);
    }
    state[key] = appId;
    return appId;
  }).catch((e) => {
    console.error("Sonnenschein: ensureStreamShortcut failed", e);
    return null;
  }).finally(() => {
    streamShortcutPromises.delete(key);
  });

  streamShortcutPromises.set(key, operation);
  return operation;
}

export async function removeLegacyStreamShortcut(state: SyncState): Promise<void> {
  // Never remove the old shortcut automatically: it may still be Steam's
  // tracked process for a stream started on v0.2.7. Dropping only our mapping
  // makes migration race-free; the already-hidden orphan is harmless.
  delete state[LEGACY_STREAM_SHORTCUT_STATE_KEY];
}

// Starts a host app through its stable hidden per-game runtime. Used both by
// the native game-details-page button and the panel's library matches.
export async function streamGame(host: HostInfo, app: HostApp): Promise<boolean> {
  try {
    const appId = await ensureStreamShortcut(host, app);
    if (appId === null) {
      return false;
    }

    const launchOptions = await getLaunchOptions(host.address, app.title);
    await SteamClient.Apps.SetAppLaunchOptions(appId, launchOptions);

    // AddShortcut resolves before Steam necessarily publishes the new entry
    // to appStore. Wait with a strict deadline so first launch succeeds
    // without ever creating a second shortcut.
    const gameId = await waitForShortcutGameId(appId);
    if (!gameId) {
      return false;
    }
    SteamClient.Apps.RunGame(gameId, "", -1, 100);
    return true;
  } catch (e) {
    console.error("Sonnenschein: streamGame failed", e);
    return false;
  }
}
