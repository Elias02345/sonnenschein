# Sonnenschein on the Steam Deck

Sonnenschein turns your Steam Deck into an extension of your gaming PC: the
games installed on your Sonnenschein host show up **inside Game Mode** and
stream at the press of a button.

The Deck setup has **two parts**:

| Part | What it does |
|---|---|
| **Sonnenschein Client** (AppImage) | The streaming app — pairing, video, audio, input |
| **Sonnenschein Decky plugin** | Game-Mode integration — puts your host's games into the Steam library and launches streams natively |

Both downloads are on the [Releases page](https://github.com/Elias02345/sonnenschein/releases).

---

## One-shot install, then pair the client

1. Switch to **Desktop Mode** (Steam button → Power → Switch to Desktop).
2. Run the installer. It downloads the latest client AppImage to
   `~/Applications`, makes it executable, and installs the matching Decky
   plugin from the same release:

   ```bash
   curl -fsSL https://github.com/Elias02345/sonnenschein/releases/latest/download/Sonnenschein-Deck-Installer.sh | bash
   ```

3. Start the client once (double-click the AppImage), select your host and
   **pair** it (enter the PIN in the host's web UI). Your host must be
   running Sonnenschein and be reachable on the local network.
4. Optional: start a test stream from the client to confirm everything works.

## Part 2 — Install the Decky plugin

1. Install [Decky Loader](https://decky.xyz/) if you don't have it yet
   (Desktop Mode → download the installer from decky.xyz and run it).
2. The one-shot command above already removes old plugin versions, installs
   the latest matching client and plugin, restarts Decky and then **verifies
   the plugin backend is actually running**. Run it again whenever you want
   to update or repair both components:

   ```bash
   curl -fsSL https://github.com/Elias02345/sonnenschein/releases/latest/download/Sonnenschein-Deck-Installer.sh | bash
   ```

   Alternative ways (no verification):
   - **Via Decky:** Quick Access menu → plug icon → gear icon → enable
     **Developer mode** → Developer → **Install plugin from zip file**.
   - **Manually:** `unzip` the release zip into `~/homebrew/plugins/`
     **without sudo**, then `sudo systemctl restart plugin_loader`.
     (Unzipping with sudo leaves root-owned files that can break the
     plugin backend — the install script cleans that up.)

3. Back in Game Mode, open the Quick Access menu → plug icon → **Sonnenschein**.
   If Steam was already running during the install, restart Steam once.

## Usage

- **Games you own on Steam** (installed on the Deck or just visible in your
  library, e.g. Portal): a small sun badge marks their library capsule when
  they are available from the paired host. Their native game page gets an extra
  **"Stream with Sonnenschein"** button in Steam's native controller-focusable
  action area next to Play/Install — no visible duplicate entries. Its dot is
  green when the host is reachable and idle, and red when the host is offline
  or already running another game. The red state is informational: pressing
  the button still attempts to wake/connect to the host and launch the game.
  Steam keeps one stable, hidden runtime identity per title, so repeated
  streams do not create new placeholder apps and each game retains its own
  Steam Input profile and overlay/focus context.
- **Host games without a Steam library entry**: the Sonnenschein panel
  **automatically syncs** them into the Steam library as entries with cover
  art. Launch them like any other game.
- Games removed from the host disappear from the Deck on the next sync.
- Leaving a stream normally also closes the streamed game on the host and
  removes the virtual display. If the network drops or the client crashes,
  the host game intentionally remains open so you can reconnect safely.
- The panel also lists all games directly for quick launching, has a manual
  re-sync button, and lets you pick the client AppImage manually if it is
  not auto-detected.

## Troubleshooting

| Symptom | Fix |
|---|---|
| Panel says the plugin backend is not responding | Run the install script from Part 2 — it reinstalls cleanly and prints the exact backend error if one occurs. Most common causes: leftover files from an old version, files unzipped with sudo, or Decky not restarted. |
| Panel says "Nicht gekoppelt" / not paired | Run the client app in Desktop Mode and pair with your host first — the plugin reuses the client's pairing. |
| Panel warns the client app is missing | Put the AppImage in `~/Applications` and make it executable. |
| A synced game won't start | Check that the host is awake and reachable; start the client app manually to verify the connection. |
| Library entries look broken after many syncs | Steam occasionally needs a restart after many shortcut changes (Steam button → Power → Restart Steam). |
| Plugin gone after a SteamOS update | SteamOS updates can remove Decky Loader — reinstall it from decky.xyz, the plugin survives in `~/homebrew/plugins`. |

## Notes

- The plugin never talks to the internet — it only talks to your paired host
  on the local network, using the client certificate created during pairing.
- Shortcut handling follows the pattern established by
  [MoonDeck](https://github.com/FrogTheFrog/moondeck) (GPL-3). Thanks!
