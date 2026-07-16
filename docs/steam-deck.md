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
2. Download `Sonnenschein-Decky-Plugin-<version>.zip` from the latest release.
3. Install the plugin — either way works:

   **Via Decky (recommended):** In Game Mode, open the Quick Access menu
   (… button) → plug icon → gear icon → enable **Developer mode** →
   Developer → **Install plugin from zip file** → select the downloaded zip.

   **Manually (Desktop Mode terminal):**

   ```bash
   unzip ~/Downloads/Sonnenschein-Decky-Plugin-*.zip -d ~/homebrew/plugins/
   sudo systemctl restart plugin_loader
   ```

4. Back in Game Mode, open the Quick Access menu → plug icon → **Sonnenschein**.

## Usage

- Opening the Sonnenschein panel **automatically syncs** your host's game
  library into the Steam library: every installed game on the host appears
  as an entry with cover art. Launch them like any other game — the stream
  starts through the Sonnenschein Client.
- Games removed from the host disappear from the Deck on the next sync.
- The panel also lists all games directly for quick launching and has a
  manual re-sync button.

## Troubleshooting

| Symptom | Fix |
|---|---|
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
