#!/usr/bin/env python3
"""Static regression contracts for Steam-internal frontend behavior."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STEAMLIB = (ROOT / "src" / "steamlib.ts").read_text(encoding="utf-8")
GAMEPAGE = (ROOT / "src" / "gamepage.tsx").read_text(encoding="utf-8")
INDEX = (ROOT / "src" / "index.tsx").read_text(encoding="utf-8")
RUNNER = (ROOT / "sonnenschein-run.sh").read_text(encoding="utf-8")
UPDATER = (ROOT / "sonnenschein-update.sh").read_text(encoding="utf-8")
BACKEND = (ROOT / "main.py").read_text(encoding="utf-8")

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
assert "data-sonnenschein-stream-anchor" in GAMEPAGE
assert 'position: "relative", height: 0' in GAMEPAGE
assert "appDetailsHeaderClasses.TopCapsule" in GAMEPAGE
assert "Math.max(0, appPanelIndex - 1)" in GAMEPAGE
assert "position: absolute !important" in GAMEPAGE

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

# The updater may accept a password only ephemerally: masked in React and
# delivered through stdin, never settings, argv, environment or logs.
assert "bIsPassword" in INDEX
assert 'useState("deck")' in INDEX
assert "setSudoPassword(\"\")" in INDEX
assert '"systemd-run"' in BACKEND
assert "input=password + \"\\n\"" in BACKEND
assert "settings[\"sudo" not in BACKEND
assert "sudo_password" not in UPDATER
assert "sha256sum -c" in UPDATER
assert "cmp -s" in UPDATER
assert "non-regular file in plugin archive" in UPDATER
assert "releases/tags/$RELEASE_TAG" in UPDATER

print("Deck frontend lifecycle contract: OK")
