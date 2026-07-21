#!/usr/bin/env python3
"""Static regression contracts for Steam-internal frontend behavior."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STEAMLIB = (ROOT / "src" / "steamlib.ts").read_text(encoding="utf-8")
GAMEPAGE = (ROOT / "src" / "gamepage.tsx").read_text(encoding="utf-8")
INDEX = (ROOT / "src" / "index.tsx").read_text(encoding="utf-8")
RUNNER = (ROOT / "sonnenschein-run.sh").read_text(encoding="utf-8")

# A populated native app map is not proof that non-Steam shortcuts finished
# loading. Persisted IDs must never be recreated from this false negative.
assert "m_mapApps.size" not in STEAMLIB
assert "return Number.isFinite(appId) && appId > 0" in STEAMLIB
assert "const pending = streamShortcutPromises.get(key)" in STEAMLIB
assert "const gameId = await waitForShortcutGameId(appId)" in STEAMLIB

# Native page matching must prefer Steam appId, and the current button must not
# re-parent Steam's own Play/Install panel.
assert "hostSteamGameIndex.get(appId)" in STEAMLIB
assert "parent.props.children[appPanelIndex] =" not in GAMEPAGE
assert "playSectionClasses.MenuButton" in GAMEPAGE

# A shortcut mapping is the only durable cleanup handle. Loading delays and
# failed removals must retain it for the next synchronization pass.
assert """if (!shortcutExists(appId)) {
          return;
        }
        try {
          await SteamClient.Apps.RemoveShortcut(appId);
        } catch (e) {
          console.error("Sonnenschein: RemoveShortcut failed", e);
          return;
        }
        delete state[key];""" in INDEX

# Every packaged runner path must request authenticated host cleanup on a
# normal stream exit.
assert RUNNER.count("--quit-after") == 3

print("Deck frontend lifecycle contract: OK")
