// Sonnenschein Decky plugin frontend.
//
// Shows the paired host's library in the Quick Access Menu and syncs host
// games into the Steam library automatically (diff-based), so games stream
// right from Game Mode. Host games that already exist as native Steam
// library entries get no duplicate shortcut — instead the native game page
// receives a "Stream via Sonnenschein" button (see gamepage.tsx) backed by
// one reusable hidden shortcut. Steam shortcut handling follows the pattern
// proven by MoonDeck (GPL-3, https://github.com/FrogTheFrog/moondeck).

import {
  ButtonItem,
  PanelSection,
  PanelSectionRow,
  staticClasses,
} from "@decky/ui";
import {
  callable,
  definePlugin,
  FileSelectionType,
  openFilePicker,
  toaster,
} from "@decky/api";
import { useEffect, useState } from "react";
import { FaSun } from "react-icons/fa";
import {
  addShortcut,
  findLibraryAppByTitle,
  launchGame,
  setRunnerPath,
  shortcutExists,
  streamGame,
  updateHostGameIndex,
} from "./steamlib";
import type { HostApp, HostInfo, SyncState } from "./steamlib";
import { patchLibraryApp } from "./gamepage";
import { startCapsuleBadges } from "./capsulebadge";

interface Status {
  clientConfFound: boolean;
  paired: boolean;
  clientAppFound: boolean;
  clientApp: string;
  appImagesFound: string[];
  runnerPath: string;
  hosts: HostInfo[];
}

const ping = callable<[], boolean>("ping");
const getStatus = callable<[], Status>("get_status");
const getApps = callable<[string, number], HostApp[]>("get_apps");
const getBoxart = callable<[string, number, string], { data: string; ext: string }>("get_boxart");
const getLaunchOptions = callable<[string, string], string>("get_launch_options");
const getState = callable<[], SyncState>("get_state");
const setState = callable<[SyncState], boolean>("set_state");
const setClientPath = callable<[string], boolean>("set_client_path");

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

async function syncHost(host: HostInfo, runnerPath: string, state: SyncState): Promise<{ added: number; removed: number; total: number }> {
  const status = { added: 0, removed: 0, total: 0 };
  const apps = await withTimeout(getApps(host.address, host.port), 20000, "Spieleliste");
  const games = apps.filter((a) => !EXCLUDED_TITLES.has(a.title));
  status.total = games.length;

  // Games that already exist as native Steam library entries get NO own
  // shortcut — they stream via the game-page button (gamepage.tsx). Leaving
  // them out of the wanted set also makes the diff below remove shortcuts
  // created before the game appeared in the library.
  const shortcutGames = games.filter((a) => findLibraryAppByTitle(a.title) === null);
  const wantedKeys = new Set(shortcutGames.map((a) => `${host.uuid}/${a.id}`));

  // Remove shortcuts for games that vanished from the host (or are now
  // matched to a native library entry)
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
  for (const app of shortcutGames) {
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

function Content() {
  const [status, setStatus] = useState<Status | null>(null);
  const [apps, setApps] = useState<HostApp[]>([]);
  const [syncState, setSyncState] = useState<SyncState>({});
  const [busy, setBusy] = useState<string>("");
  const [error, setError] = useState<string>("");
  const [streamingTitle, setStreamingTitle] = useState<string>("");

  const refresh = async (autoSync: boolean) => {
    try {
      setError("");
      setBusy("Verbinde mit Backend…");

      // Liveness first: a dead backend gets a precise message instead of a
      // silently stuck panel. Three attempts so a backend that is still
      // starting (fresh install, loader restart) gets a fair chance.
      let backendAlive = false;
      for (let attempt = 1; attempt <= 3 && !backendAlive; attempt++) {
        try {
          setBusy(attempt === 1 ? "Verbinde mit Backend…" : `Backend startet… (Versuch ${attempt}/3)`);
          await withTimeout(ping(), 6000, "Backend-Ping");
          backendAlive = true;
        } catch (e) {
          console.error(`Sonnenschein: ping attempt ${attempt} failed`, e);
        }
      }
      if (!backendAlive) {
        throw new Error(
          "Plugin-Backend antwortet nicht. Lösung: Im Desktop-Modus das Install-Script aus der Anleitung (docs/steam-deck.md) ausführen — es installiert sauber neu und zeigt den genauen Fehler an. Danach das Deck neu starten."
        );
      }

      setBusy("Lese Client-Konfiguration…");
      const s = await withTimeout(getStatus(), 10000, "Status");
      setStatus(s);
      setRunnerPath(s.runnerPath);
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
      const games = hostApps.filter((a) => !EXCLUDED_TITLES.has(a.title));
      // Keep the game-page button's index in sync with what the panel shows.
      updateHostGameIndex(host, games);
      setApps(games);
      setSyncState({ ...state });
      setBusy("");
    } catch (e: any) {
      console.error("Sonnenschein: refresh failed", e);
      setBusy("");
      setError(`${e?.message ?? e}`);
    }
  };

  const pickClientApp = async () => {
    try {
      const res = await openFilePicker(FileSelectionType.FILE, "/home/deck", true, false);
      if (!res?.path) {
        return;
      }
      setBusy("Speichere Client-App-Pfad…");
      const ok = await withTimeout(setClientPath(res.path), 10000, "Client-Pfad speichern");
      if (!ok) {
        throw new Error("Backend hat den Pfad nicht akzeptiert.");
      }
      await refresh(false);
    } catch (e: any) {
      // A cancelled picker rejects its promise — not an error worth showing.
      const msg = `${e?.message ?? e}`;
      if (/cancel/i.test(msg)) {
        return;
      }
      console.error("Sonnenschein: pickClientApp failed", e);
      setBusy("");
      setError(msg);
    }
  };

  const startStream = async (host: HostInfo, app: HostApp) => {
    setStreamingTitle(app.title);
    try {
      const ok = await streamGame(host, app.title);
      if (!ok) {
        toaster.toast({ title: "Sonnenschein", body: `${app.title} konnte nicht gestartet werden.` });
      }
    } finally {
      setStreamingTitle("");
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
          <>
            <PanelSectionRow>
              ⚠ Client-App nicht gefunden — lege die AppImage in ~/Applications ab
              oder wähle sie manuell aus.
            </PanelSectionRow>
            {status.appImagesFound.length > 0 && (
              <PanelSectionRow>
                <div style={{ fontSize: "0.75em", wordBreak: "break-all" }}>
                  Gefundene AppImages:
                  {status.appImagesFound.map((p) => (
                    <div key={p}>{p}</div>
                  ))}
                </div>
              </PanelSectionRow>
            )}
            <PanelSectionRow>
              <ButtonItem layout="below" onClick={pickClientApp} disabled={!!busy}>
                Client-App auswählen…
              </ButtonItem>
            </PanelSectionRow>
          </>
        )}
        {status.clientAppFound && !!status.clientApp && (
          <PanelSectionRow>
            <div style={{ fontSize: "0.7em", opacity: 0.6, wordBreak: "break-all" }}>
              Client-App: {status.clientApp}
            </div>
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
          // Games with a native Steam library entry stream via the shared
          // hidden shortcut; the rest launch their own synced shortcut.
          const libraryMatch = findLibraryAppByTitle(app.title);
          const steamAppId = syncState[`${host.uuid}/${app.id}`];
          const canLaunch = libraryMatch !== null || !!steamAppId;
          return (
            <PanelSectionRow key={app.id}>
              <ButtonItem
                layout="below"
                onClick={() => {
                  if (libraryMatch) {
                    startStream(host, app);
                  } else if (steamAppId) {
                    launchGame(steamAppId);
                  }
                }}
                disabled={!canLaunch || streamingTitle === app.title}
              >
                {streamingTitle === app.title ? `${app.title} — startet…` : app.title}
              </ButtonItem>
            </PanelSectionRow>
          );
        })}
      </PanelSection>
    </>
  );
}

const LIBRARY_APP_ROUTE = "/library/app/:appid";

export default definePlugin(() => {
  // Game-page button on /library/app/:appid — MoonDeck-style route patch.
  // Steam UI drift must never prevent the QAM panel/backend from loading.
  let removeLibraryPatch: (() => void) | null = null;
  try {
    removeLibraryPatch = patchLibraryApp(LIBRARY_APP_ROUTE);
  } catch (e) {
    console.error("Sonnenschein: game-page route patch failed", e);
  }
  const stopCapsuleBadges = startCapsuleBadges();

  // Populate the host game index right at plugin load so the game-page
  // button works before the QAM panel was ever opened.
  (async () => {
    try {
      const s = await withTimeout(getStatus(), 10000, "Initialer Status");
      setRunnerPath(s.runnerPath);
      if (s.paired && s.hosts.length > 0) {
        const host = s.hosts[0];
        const hostApps = await withTimeout(getApps(host.address, host.port), 20000, "Initiale Spieleliste");
        updateHostGameIndex(host, hostApps.filter((a) => !EXCLUDED_TITLES.has(a.title)));
      }
    } catch (e) {
      console.error("Sonnenschein: initial host game index load failed", e);
    }
  })();

  return {
    name: "Sonnenschein",
    titleView: <div className={staticClasses.Title}>Sonnenschein</div>,
    content: <Content />,
    icon: <FaSun />,
    onDismount() {
      removeLibraryPatch?.();
      stopCapsuleBadges();
    },
  };
});
