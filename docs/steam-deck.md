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

## Part 1 — Install the Sonnenschein Client

1. Switch to **Desktop Mode** (Steam button → Power → Switch to Desktop).
2. Download `Sonnenschein_Client-<version>-x86_64.AppImage` from the latest
   release.
3. Move it to `~/Applications` (create the folder if it doesn't exist) and
   make it executable:

   ```bash
   mkdir -p ~/Applications
   mv ~/Downloads/Sonnenschein_Client-*.AppImage ~/Applications/
   chmod +x ~/Applications/Sonnenschein_Client-*.AppImage
   ```

   > The Decky plugin looks for the client in `~/Applications` — keep it there.

4. Start the client once (double-click the AppImage), select your host and
   **pair** it (enter the PIN in the host's web UI). Your host must be
   running Sonnenschein and be reachable on the local network.
5. Optional: start a test stream from the client to confirm everything works.

## Part 2 — Install the Decky plugin

1. Install [Decky Loader](https://decky.xyz/) if you don't have it yet
   (Desktop Mode → download the installer from decky.xyz and run it).
2. **Recommended: the install script** (Desktop Mode → Konsole). It removes
   old plugin versions, downloads the latest release, installs it with
   correct permissions, restarts Decky and then **verifies the plugin
   backend is actually running** (showing the exact error if not):

   ```bash
   curl -fsSL https://raw.githubusercontent.com/Elias02345/sonnenschein/dev/decky-plugin/deck-install.sh | bash
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
  library, e.g. Portal): their native game page gets an extra
  **"Stream via Sonnenschein"** button next to Play/Install — no duplicate
  entries. Press it and the game streams from your host; it feels like a
  native launch.
- **Host games without a Steam library entry**: the Sonnenschein panel
  **automatically syncs** them into the Steam library as entries with cover
  art. Launch them like any other game.
- Games removed from the host disappear from the Deck on the next sync.
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
