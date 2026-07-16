// Sonnenschein Decky plugin frontend.
//
// Shows the paired host's library in the Quick Access Menu and syncs every
// host game into the Steam library automatically (diff-based), so games
// stream right from Game Mode. Steam shortcut handling follows the pattern
// proven by MoonDeck (GPL-3, https://github.com/FrogTheFrog/moondeck).

import {
  ButtonItem,
  PanelSection,
  PanelSectionRow,
  staticClasses,
} from "@decky/ui";
import { callable, definePlugin, toaster } from "@decky/api";
import { useEffect, useState } from "react";
import { FaSun } from "react-icons/fa";

interface HostInfo {
  name: string;
  uuid: string;
  address: string;
  port: number;
}

interface Status {
  clientConfFound: boolean;
  paired: boolean;
  clientAppFound: boolean;
  runnerPath: string;
  hosts: HostInfo[];
}

interface HostApp {
  id: string;
  title: string;
}

type SyncState = Record<string, number>; // "<hostUuid>/<appId>" -> steam appId

const ping = callable<[], boolean>("ping");
const getStatus = callable<[], Status>("get_status");
const getApps = callable<[string, number], HostApp[]>("get_apps");
const getBoxart = callable<[string, number, string], { data: string; ext: string }>("get_boxart");
const getLaunchOptions = callable<[string, string], string>("get_launch_options");
const getState = callable<[], SyncState>("get_state");
const setState = callable<[SyncState], boolean>("set_state");

// A backend that never answers must not leave the panel stuck — every
// backend call runs against a deadline.
function withTimeout<T>(p: Promise<T>, ms: number, label: string): Promise<T> {
  return Promise.race([
    p,
    new Promise<T>((_, reject) =>
      setTimeout(() => reject(new Error(`${label}: Zeitüberschreitung (${ms / 1000}s)`)), ms)
    ),
  ]);
}

// Non-stream entries of the unified list we do not want in the library
const EXCLUDED_TITLES = new Set(["Desktop", "Steam Big Picture"]);

declare const SteamClient: any;
declare const appStore: any;

async function addShortcut(name: string, exe: string): Promise<number | null> {
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

function shortcutExists(appId: number): boolean {
  try {
    return !!appStore.GetAppOverviewByAppID(appId);
  } catch (_) {
    return false;
  }
}

async function syncHost(host: HostInfo, runnerPath: string, state: SyncState): Promise<{ added: number; removed: number; total: number }> {
  const status = { added: 0, removed: 0, total: 0 };
  const apps = await withTimeout(getApps(host.address, host.port), 20000, "Spieleliste");
  const games = apps.filter((a) => !EXCLUDED_TITLES.has(a.title));
  status.total = games.length;

  const wantedKeys = new Set(games.map((a) => `${host.uuid}/${a.id}`));

  // Remove shortcuts for games that vanished from the host
  for (const key of Object.keys(state)) {
    if (key.startsWith(`${host.uuid}/`) && !wantedKeys.has(key)) {
      const appId = state[key];
      try {
        if (shortcutExists(appId)) {
          await SteamClient.Apps.RemoveShortcut(appId);
        }
      } catch (e) {
        console.error("Sonnenschein: RemoveShortcut failed", e);
      }
      delete state[key];
      status.removed++;
    }
  }

  // Add missing shortcuts
  for (const app of games) {
    const key = `${host.uuid}/${app.id}`;
    if (state[key] && shortcutExists(state[key])) {
      continue;
    }

    const appId = await addShortcut(app.title, runnerPath);
    if (appId === null) {
      continue;
    }

    const launchOptions = await getLaunchOptions(host.address, app.title);
    try {
      await SteamClient.Apps.SetAppLaunchOptions(appId, launchOptions);
    } catch (e) {
      console.error("Sonnenschein: SetAppLaunchOptions failed", e);
    }

    // Box art (portrait grid) from the host's Steam cache
    try {
      const art = await withTimeout(getBoxart(host.address, host.port, app.id), 15000, "Boxart");
      if (art.data) {
        await SteamClient.Apps.SetCustomArtworkForApp(appId, art.data, art.ext, 0);
      }
    } catch (e) {
      console.error("Sonnenschein: SetCustomArtworkForApp failed", e);
    }

    state[key] = appId;
    status.added++;
  }

  await setState(state);
  return status;
}

function launchGame(steamAppId: number) {
  try {
    const overview = appStore.GetAppOverviewByAppID(steamAppId);
    const gameId = overview?.gameid ?? overview?.m_gameid;
    if (!gameId) {
      toaster.toast({ title: "Sonnenschein", body: "Shortcut not found — try re-syncing." });
      return;
    }
    SteamClient.Apps.RunGame(gameId, "", -1, 100);
  } catch (e) {
    console.error("Sonnenschein: RunGame failed", e);
  }
}

function Content() {
  const [status, setStatus] = useState<Status | null>(null);
  const [apps, setApps] = useState<HostApp[]>([]);
  const [syncState, setSyncState] = useState<SyncState>({});
  const [busy, setBusy] = useState<string>("");
  const [error, setError] = useState<string>("");

  const refresh = async (autoSync: boolean) => {
    try {
      setError("");
      setBusy("Verbinde mit Backend…");

      // Liveness first: a dead backend gets a precise message instead of a
      // silently stuck panel.
      try {
        await withTimeout(ping(), 6000, "Backend-Ping");
      } catch (e: any) {
        throw new Error(
          "Plugin-Backend antwortet nicht. Decky-Log prüfen: /home/deck/homebrew/logs/Sonnenschein/ — ggf. Plugin neu installieren und Steam neu starten."
        );
      }

      setBusy("Lese Client-Konfiguration…");
      const s = await withTimeout(getStatus(), 10000, "Status");
      setStatus(s);
      if (!s.paired || s.hosts.length === 0) {
        setBusy("");
        return;
      }
      const host = s.hosts[0];
      const state = await withTimeout(getState(), 10000, "Sync-Status");

      if (autoSync) {
        setBusy("Synchronisiere Bibliothek…");
        const result = await syncHost(host, s.runnerPath, state);
        if (result.added || result.removed) {
          toaster.toast({
            title: "Sonnenschein",
            body: `Bibliothek aktualisiert: ${result.added} neu, ${result.removed} entfernt`,
          });
        }
      }

      setBusy("Lade Spieleliste…");
      const hostApps = await withTimeout(getApps(host.address, host.port), 20000, "Spieleliste");
      setApps(hostApps.filter((a) => !EXCLUDED_TITLES.has(a.title)));
      setSyncState({ ...state });
      setBusy("");
    } catch (e: any) {
      console.error("Sonnenschein: refresh failed", e);
      setBusy("");
      setError(`${e?.message ?? e}`);
    }
  };

  useEffect(() => {
    // Auto-sync on panel open (maintainer decision: full auto sync)
    refresh(true);
  }, []);

  if (error) {
    return (
      <PanelSection title="Fehler">
        <PanelSectionRow>{error}</PanelSectionRow>
        <PanelSectionRow>
          <ButtonItem layout="below" onClick={() => refresh(true)}>
            Erneut versuchen
          </ButtonItem>
        </PanelSectionRow>
      </PanelSection>
    );
  }

  if (!status) {
    return (
      <PanelSection>
        <PanelSectionRow>{busy || "Lade…"}</PanelSectionRow>
      </PanelSection>
    );
  }

  if (!status.paired) {
    return (
      <PanelSection title="Nicht gekoppelt">
        <PanelSectionRow>
          Installiere die Sonnenschein-Client-App und kopple sie im Desktop-Modus
          mit deinem Host. Das Plugin übernimmt das Pairing automatisch.
        </PanelSectionRow>
      </PanelSection>
    );
  }

  const host = status.hosts[0];

  return (
    <>
      <PanelSection title={host.name}>
        {!status.clientAppFound && (
          <PanelSectionRow>
            ⚠ Client-App nicht gefunden — lege die AppImage in ~/Applications ab.
          </PanelSectionRow>
        )}
        {busy && <PanelSectionRow>{busy}</PanelSectionRow>}
        <PanelSectionRow>
          <ButtonItem layout="below" onClick={() => refresh(true)} disabled={!!busy}>
            Bibliothek neu synchronisieren
          </ButtonItem>
        </PanelSectionRow>
      </PanelSection>

      <PanelSection title={`Spiele (${apps.length})`}>
        {apps.map((app) => {
          const steamAppId = syncState[`${host.uuid}/${app.id}`];
          return (
            <PanelSectionRow key={app.id}>
              <ButtonItem
                layout="below"
                onClick={() => steamAppId && launchGame(steamAppId)}
                disabled={!steamAppId}
              >
                {app.title}
              </ButtonItem>
            </PanelSectionRow>
          );
        })}
      </PanelSection>
    </>
  );
}

export default definePlugin(() => {
  return {
    name: "Sonnenschein",
    titleView: <div className={staticClasses.Title}>Sonnenschein</div>,
    content: <Content />,
    icon: <FaSun />,
    onDismount() {},
  };
});
