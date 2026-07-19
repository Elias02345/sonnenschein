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

// "<hostUuid>/<appId>" -> steam appId, plus the special key below.
export type SyncState = Record<string, number>;

// Special key in the shared sync-state blob for the one reusable, hidden
// non-Steam shortcut used to stream library games (games that already have
// a native Steam library entry get no per-game shortcut — see
// ensureStreamShortcut).
const STREAM_SHORTCUT_STATE_KEY = "_streamShortcut";
const STREAM_SHORTCUT_NAME = "Sonnenschein Stream";

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

export function updateHostGameIndex(host: HostInfo, apps: HostApp[]): void {
  hostGameIndex.clear();
  for (const app of apps) {
    const key = normalizeTitle(app.title);
    if (key) {
      hostGameIndex.set(key, { host, app });
    }
  }
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

// One reusable, hidden non-Steam shortcut for streaming games that already
// have a native library entry — never a per-game shortcut for these.
export async function ensureStreamShortcut(): Promise<number | null> {
  try {
    if (!cachedRunnerPath) {
      // Runner path unknown (initial status load failed) — a shortcut with
      // an empty exe would be permanently broken.
      console.error("Sonnenschein: runner path not loaded yet");
      return null;
    }
    const state = await getState();
    const existingId = state[STREAM_SHORTCUT_STATE_KEY];
    if (typeof existingId === "number" && shortcutExists(existingId)) {
      return existingId;
    }

    const appId = await addShortcut(STREAM_SHORTCUT_NAME, cachedRunnerPath);
    if (appId === null) {
      return null;
    }
    try {
      collectionStore.SetAppsAsHidden([appId], true);
    } catch (e) {
      console.error("Sonnenschein: hiding stream shortcut failed", e);
    }

    state[STREAM_SHORTCUT_STATE_KEY] = appId;
    await setState(state);
    return appId;
  } catch (e) {
    console.error("Sonnenschein: ensureStreamShortcut failed", e);
    return null;
  }
}

// Starts `title` on `host` through the shared stream shortcut. Used both by
// the game-details-page button and by the panel's list for library matches.
export async function streamGame(host: HostInfo, title: string): Promise<boolean> {
  try {
    const appId = await ensureStreamShortcut();
    if (appId === null) {
      return false;
    }

    const launchOptions = await getLaunchOptions(host.address, title);
    await SteamClient.Apps.SetAppLaunchOptions(appId, launchOptions);

    const overview = appStore.GetAppOverviewByAppID(appId);
    const gameId = overview?.gameid ?? overview?.m_gameid;
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
