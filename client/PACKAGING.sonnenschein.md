# sonnenschein-client — Test-Apps (Windows & Steam Deck)

Der Fork erbt Moonlight-Qts Build-Infrastruktur (`scripts/`, `.github/workflows/`).
**GitHub Actions ist aktiv** und baut bei jedem Push automatisch:
- **Windows** (`build-win-mac.yml`) — ✅ grün,
- **AppImage** (Linux, läuft portabel auf SteamOS/Deck) — nach dem Rebrand-Fix grün,
- (Steam-Link-Job wurde entfernt — veraltete Hardware, kein Sonnenschein-Ziel).

## Test-Apps herunterladen (am einfachsten)
1. GitHub → Repo `sonnenschein-client` → **Actions** → letzter grüner „Build"-Run.
2. Unten unter **Artifacts**: Windows-`.exe`-Bundle + `.AppImage` herunterladen.
3. **Windows**: entpacken, `.exe` starten. **Deck**: `.AppImage` aufs Deck kopieren,
   im Desktop Mode ausführbar machen (`chmod +x`) und starten.

## Als GitHub-**Release/Version** veröffentlichen (downloadbare Pakete)
Artifacts verfallen nach ~90 Tagen. Für dauerhafte, versionierte Downloads einen
Tag pushen — dann kann die CI ein Release mit den Binaries anhängen:
```bash
git tag v0.1.0-test && git push origin v0.1.0-test
```
(Release-Publishing-Step ist vorbereitbar; sag Bescheid, dann hänge ich die
Artifacts automatisch ans Release — dafür brauche ich nur dein OK.)

## Lokal bauen (falls gewünscht)
- **Deck-AppImage** auf einem Linux-Rechner: `linuxdeploy` + `linuxdeploy-plugin-qt`
  holen (AppImages, kein sudo), Deps installieren, dann `bash scripts/build-appimage.sh`.
- **Windows** auf deiner Windows-Maschine: Visual Studio + Qt 6, dann
  `scripts\build-arch.bat release x64`.

## Rebrand-Hinweise
- Linux-Binary/`.desktop`-`Exec` = `sonnenschein-client`; QSettings-Identität
  `Sonnenschein/sonnenschein-client`. Windows-`TARGET` ist noch `Moonlight`
  (für eigenen Namen `app/app.pro` anpassen).
- **SDL2_ttf**: die CI baut es selbst; lokal auf CachyOS liegt es unter
  `~/Dokumente/.localdeps` (ohne sudo gebaut).

## Testbar mit den Apps
`detect-profile` (Auto-Konfig), `library`/`library-gui` (Host-Bibliothek),
Pairing + Stream, Remote-Desktop-Modus (Single-Monitor / Absolut).
