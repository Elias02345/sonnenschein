# Sonnenschein Roadmap

Lebendes Dokument. Entscheidungen, die in Issues und PRs getroffen werden, fließen hier zurück.

> **Quelle der Wahrheit für den tagesaktuellen Stand ist [`STATUS.md`](STATUS.md).** Diese
> Roadmap zeichnet die langfristige Phasen-Struktur, die Checkboxen hinken bewusst etwas hinterher.

## Vision

Sonnenschein soll der **Standard-Weg** sein, einen Linux-PC oder -Server in einen vollwertigen, headless Game-Streaming-Host zu verwandeln, der jeden Moonlight-Client mit perfekter Auflösung, Bildwiederholrate und (optional) HDR bedient. Setup darf keine Minute Troubleshooting kosten — Installer-Skript, automatische Konfiguration, automatische Updates.

**Erweiterte Vision (beschlossen 2026-07-11):** Nach der Host-1.0 baut Sonnenschein einen **eigenen Client** (Linux/Steam Deck zuerst, dann Android und Windows) mit automatischer Geräte-Konfiguration und zwei Profilen (Gaming / Remote Desktop) — und als Killer-Feature die **native Steam-Deck-Integration**: Die Spielebibliothek des gepairten Hosts erscheint direkt im Steam Game Mode des Decks, lokal installierte Spiele starten lokal, Host-Spiele starten als Stream — alles über Sonnenschein, nicht über Steam Remote Play. Der Host ist dabei **boot-to-ready**: startet leise als Service, wartet lightweight auf Verbindungen, ist per Wake-on-LAN von überall aufweckbar.

## Leitprinzipien

1. **Wayland-first.** X11 wird unterstützt, ist aber nicht das Primärziel. KDE Plasma 6 ist der Hauptfokus.
2. **Wir verstecken Komplexität, wir versimpfeln sie nicht.** Wenn ein Linux-Streaming-Host kompliziert ist, lösen wir die einzelnen Probleme — wir verlangen sie nicht vom Nutzer.
3. **Ein Pfad pro Vendor, automatisch gewählt.** Multi-Backend ist intern, der Nutzer sieht "es funktioniert".
4. **Robust > schnell.** Wir bauen lieber langsam ein stabiles Fundament als ein hektisches MVP, das beim ersten Distro-Wechsel zerbricht.
5. **Open Source, transparent, kollaborativ.** GPL-3.0, klare Issue-Templates, dokumentierte Architektur.

## Phasen

### Phase 0 — Vorbereitung *(abgeschlossen)*

**Ziel**: Repo strukturiert, brandgesetzt, CI-Skelett, Community-Files. Apollo-Codebasis als Ausgangspunkt importiert.

**Erfolgskriterium**: `git clone` + `cmake --preset linux` baut ohne Patches auf einem aktuellen Arch-System (innerhalb WSL2 oder Container).

- [x] Apollo-Repo als Basis importieren
- [x] LICENSE (GPL-3.0) + NOTICE (Attribution Sunshine + Apollo)
- [x] README (DE/EN)
- [x] Roadmap
- [ ] CONTRIBUTING.md
- [ ] CODE_OF_CONDUCT.md
- [ ] Issue Templates (Bug, Feature, Crash-Report)
- [ ] PR-Template
- [ ] GitHub Actions: Lint + Build-Matrix für Arch / Ubuntu / Fedora
- [ ] package.json: `name` → `sonnenschein`
- [ ] Erstes Tag: `v0.0.0-phase0`

### Phase 1 — Linux-Build *(abgeschlossen — 270/270 Tests)*

**Ziel**: Apollo-Codebasis baut auf Linux ohne Apollo-spezifische Windows-Pfade. Sunshine-Patches der letzten Monate werden gemerged.

**Erfolgskriterium**: `sonnenschein` startet auf Arch + Ubuntu + Fedora, WebUI auf 47990 erreichbar, Pairing mit Moonlight-Client funktioniert auf einem laufenden Desktop-Display (noch ohne Virtual-Display-Magie).

- [x] CMakeLists `project(Sonnenschein)` — sukzessive Rename
- [x] `cmake/prep/constants.cmake` — Pfade an Sonnenschein anpassen
- [ ] Sunshine-Upstream-Sync-Script (cherry-pick automatisch)
- [x] systemd-Service-File: `sonnenschein-server.service`
- [x] WebUI build (Vite) im CMake-Target
- [x] Erster grüner CI-Run mit Build-Artefakten

### Phase 2 — Virtual-Display-Abstraktion *(Kern-Innovation)*

**Ziel**: Beim Pairing wird automatisch ein Virtual Display in der vom Client geforderten Auflösung+Refresh+HDR-Kapazität erstellt. Andere Outputs werden bei Bedarf abgeschaltet (User-Toggle in WebUI).

**Erfolgskriterium**: Auf KDE Plasma 6 Wayland + AMD oder NVIDIA → Pairing erstellt ein neues Output, Spiel rendert dort, Stream zeigt es korrekt. Idem für GNOME Mutter Headless.

- [x] `src/platform/linux/virtual_display/interface.h` — die Abstraktions-Schicht
- [ ] Backend: AMDGPU `virtual_display=` Kernel-Param (Boot-Setup im Installer)
- [ ] Backend: Mutter `--headless --virtual-monitor` via D-Bus
- [x] Backend: KWin Virtual Output (kscreen-doctor + KWin-D-Bus, ggf. KWin-Plugin) — 🟡 CachyOS-Test offen
- [ ] Backend: wlroots Headless (Sway / Hyprland)
- [ ] Backend: Xorg dummy + xrandr CVT-reduced
- [ ] Backend: Xorg NVIDIA EDID-Spoof
- [ ] Backend: EVDI als Last Resort
- [x] Auswahl-Heuristik (Compositor-Detection + GPU-Detection)
- [x] Per-Client GUID-Mapping (aus Apollo `process.cpp:285-330` adaptiert)
- [x] Multi-Display-Cleanup beim Disconnect — 🟡 CachyOS-Test offen (60-Hz/HDR/Cursor)

### Phase 3 — Installer & Service

**Ziel**: Ein Bash-Skript, mehrere Distro-Pfade, robust gegen alle möglichen Konfigurationen, mit klaren Fehlermeldungen.

**Erfolgskriterium**: Auf vier frisch installierten VMs (Arch, Ubuntu 24.04, Fedora 41, openSUSE Tumbleweed) macht `curl ... | bash` jeweils eine funktionierende Sonnenschein-Instanz.

- [x] `installer/install.sh` — Entry, Distro-/GPU-/Compositor-Detection
- [x] `installer/lib/distro.sh` — `detect_distro()`
- [x] `installer/lib/gpu.sh` — `detect_gpu()` (lspci/nvidia-smi-basiert)
- [x] `installer/lib/compositor.sh` — Wayland/X11 + KWin/Mutter/Sway
- [x] `installer/lib/packages.sh` — distrospezifische Paket-Listen
- [x] `installer/lib/service.sh` — systemd-Unit-Setup (`--user`-Default)
- [x] `installer/lib/permissions.sh` — udev-Rules, Group-Memberships, capabilities
- [x] `installer/lib/ui.sh` — Text-UI-Helfer
- [x] systemd-Units: `sonnenschein-server.service`, optional `sonnenschein-display.service`
- [x] `installer/uninstall.sh` — sauberes Entfernen aller Komponenten
- [x] `installer/post-install.sh` — Pairing-PIN, WebUI-URL ausgeben
- [ ] Idempotenz-Tests auf echten Distro-VMs (🟡 Maintainer-Test offen)

### Phase 4 — HDR & AV1

**Ziel**: HDR10-Streaming funktioniert auf KDE Plasma 6 Wayland mit AMD-RDNA3 und NVIDIA-Driver-580+. AV1 wird priorisiert, wenn der Client+GPU es können.

**Erfolgskriterium**: Moonlight zeigt HDR10-Indikator und liefert ein farbkorrektes Bild auf einem HDR-Display.

- [ ] Capability-Negotiation aus Apollo (`nvhttp.cpp:960-989`) übernehmen
- [ ] VAAPI HDR-Encoder-Pfad (Sunshine ist hier nur Stub)
- [ ] NVENC HDR-Encoder-Pfad (Apollo-Code adaptieren)
- [ ] Vulkan Video Encode (experimentell)
- [ ] Compositor-Side: HDR-Output-Aktivierung pro Stream (KWin / Mutter)
- [ ] Driver-Versionsprüfung im Installer + Warnung bei <580 (NVIDIA)
- [ ] AV1-Preference per WebUI-Schalter

### Phase 5 — WebUI v1

**Ziel**: Eine moderne, gamer-taugliche Web-UI mit Setup-Wizard. Vue 3 + PrimeVue 4. Vollständig deutsch + englisch. Dunkelmodus-Default. Responsive bis 360 px Mobile.

**Erfolgskriterium**: Ein Nutzer ohne Vorwissen kann nach Installer-Run die WebUI öffnen, in <5 Minuten ein erstes Pairing durchführen und ein Spiel streamen — ohne Terminal anzufassen.

- [x] Vue 3 + PrimeVue 4 + vue-i18n Setup (Fundament: `primevue_init.js`, Aura-Preset, Dark-Default)
- [ ] Login-Screen (mit Browser-Passwort-Save)
- [ ] Setup-Wizard: Pairing-PIN-Generator, GPU-Bestätigung, Compositor-Test, erstes Spiel hinzufügen
- [ ] Dashboard: Live-GPU-Auslastung, aktuelle Streams, Display-Zustand
- [ ] Apps-Verwaltung (wie Sunshine + Per-App-Gamescope-Toggle + Per-App-HDR-Override)
- [ ] Geräte-Verwaltung (gepairte Clients, Permissions)
- [x] Diagnose-Tab: Compositor, GPU, Treiber, Kernel, Mesa-Version, HDR-Capability (🟡 Browser-Test offen)
- [ ] Live-Log-Tab (journald-Stream)
- [ ] Update-Manager (siehe Phase 6)
- [ ] Crash-Reporter (siehe unten)
- [ ] Theme: Dunkelmodus-Default, optional Hell, System-Default-Folger
- [ ] Animationen: hochwertig, nicht aufdringlich (PrimeVue + AutoAnimate o. Ä.)
- [ ] i18n: vue-i18n, DE + EN ab 1.0, Crowdin-Setup für Community

### Phase 6 — Update-System

**Ziel**: Sonnenschein hält sich selbst aktuell — ohne dass der Nutzer manuelle Pakete jagen muss.

**Erfolgskriterium**: WebUI zeigt "Neue Version verfügbar", ein Klick → Service stoppt, neue Version lädt, Migration läuft, Service startet, alle Configs+Pairings sind heile. CLI-Befehl `sonnenschein update` macht dasselbe headless.

- [x] `update.sh` — Source-Pull + Rebuild + Reinstall + Service-Restart + doctor --repair (2026-07)
- [x] WebUI-Update-Knopf — `POST /api/update` spawnt den Updater via `systemd-run --user` (überlebt den Service-Restart); Dashboard-Button erscheint bei neuer Version. Ohne NOPASSWD-sudo liefert der Endpoint den manuellen Befehl. (2026-07-12)
- [ ] Branch-Wahl (`main` / `dev`) mit Verhalten: wenn `main` neuer als `dev` → automatisch zu `main` (API nimmt `branch` schon entgegen, UI-Selector fehlt)
- [ ] Migration-Framework: pro Major-Version ein Up-Script, Backups vor jedem Lauf
- [ ] Auto-Update als Service-Option (timer-basiert) oder On-Demand
- [ ] Rollback-Befehl

### Phase 7 — Tests, Performance, Dokumentation

**Ziel**: Genug Reife für eine 1.0.

**Erfolgskriterium**: Min. 4 Distros × 3 GPUs × 2 Compositoren in CI-Smoketests. <30 ms Latenz LAN. Doku decken alle Setup-Szenarien ab.

- [ ] CI: Smoketests in qemu/podman pro Distro
- [ ] Latenz-Benchmark-Suite (Frame-Trace, ENet-Probe)
- [ ] mkdocs-material Dokumentation (übernommen von Sunshine, neu strukturiert)
- [ ] Setup-Videos (DE + EN, optional)
- [ ] Troubleshooting-Knowledge-Base

### Phase 8 — 1.0 Release

**Ziel**: Native Pakete für die Top-Distros, klares Marketing.

- [ ] AUR `sonnenschein-bin` und `sonnenschein-git`
- [ ] COPR (Fedora)
- [ ] PPA (Ubuntu)
- [ ] Flatpak (flathub-Submission)
- [ ] AppImage als Fallback
- [ ] Release-Announcement (r/linux_gaming, r/Steamdeck, etc.)

---

## Host-Bereitschaft: Boot-to-Ready (Querschnitt, beginnt in Phase 3/7)

**Ziel**: Der Host ist nach dem Einschalten streambar, ohne dass jemand am PC sitzt. Leise, lightweight, von überall aufweckbar — und trotzdem geschützt (Pairing-Zertifikate + PIN bleiben die Zugangskontrolle).

**Stufenplan** (jede Stufe einzeln nutzbar):

1. **Leiser Autostart** *(heute erreicht)* — systemd `--user`-Service startet mit der Plasma-Session (`xdg-desktop-autostart.target`), idlet ohne Stream nur mit mDNS-Announce + HTTPS-Listener (kein GPU-Zugriff bis Stream-Start).
2. **Wake-on-LAN** — Installer/doctor konfigurieren `ethtool -s <iface> wol g` persistent (systemd-Unit); WebUI zeigt die MAC-Adresse; das Moonlight-Protokoll unterstützt WoL-Magic-Packets bereits, unser Client (siehe Client-Track) bekommt einen prominenten „Host aufwecken"-Knopf. Fernzugriff übers Internet: dokumentierter Weg via VPN (Tailscale/WireGuard empfohlen — WoL-Broadcasts überqueren kein NAT).
   - [ ] `installer/lib/wol.sh`: NIC-Detection, `wol g` persistent setzen, BIOS-Hinweis
   - [ ] doctor-Check: „WoL armed?" + MAC-Anzeige
   - [ ] WebUI: MAC + Anleitung im Diagnose-Tab
3. **Login-freies Streamen (opt-in)** — SDDM-Autologin per Installer-Toggle (mit klarem Sicherheitshinweis: Vollverschlüsselung empfohlen; die Session ist nach Boot entsperrt, Zugriff auf den Stream bleibt durch Pairing geschützt). Physische Monitore bleiben dank Headless-Modus ohnehin aus.
   - [ ] Installer-Option `--autologin` (SDDM-Konfiguration, revertierbar, im Manifest)
4. **Forschung: dedizierte Headless-Session** — Sonnenschein startet selbst eine minimale `kwin_wayland --virtual`-Session als System-Service, ganz ohne Autologin/SDDM. Sauberste Lösung, aber KWin-seitig anspruchsvoll (Session-DBus, PipeWire-User-Instanz). Erst nach Client-Track M1 evaluieren.

---

## Client-Track: Der Sonnenschein-Client *(beschlossen 2026-07-11 — ersetzt das frühere Kein-Ziel „keine eigene Client-App")*

**Warum eigener Client**: Moonlight ist ein exzellenter generischer Player, aber er weiß nichts über Geräte-Eigenheiten (Deck-Panel, Refresh, HDR-Fähigkeit) und nichts über die Host-Bibliothek. Der Sonnenschein-Client macht aus „Streaming-App einrichten" ein „installieren und losspielen" — und verwandelt das Steam Deck in eine echte Erweiterung des Gaming-PCs.

**Prioritäten**: Gaming vor Remote Desktop. Innerhalb Gaming: **native Steam-Deck-Anbindung zuerst**, dann Android (TV/Handheld), dann Windows.

**Technische Basis**: Fork von Moonlight-Qt (GPL-3, Linux/Windows/SteamOS aus einer Codebasis; für Android später Fork von moonlight-android). Wir erben damit ein gereiftes Protokoll + Decoder/Renderer und konzentrieren uns auf die Sonnenschein-Mehrwerte. Protokoll bleibt Moonlight-kompatibel — Fremd-Clients funktionieren weiter.

### C1 — Client-Fundament (Linux + SteamOS)

**Ziel**: `sonnenschein-client` läuft auf Linux-Desktop und Steam Deck (Flatpak), verbindet sich mit einem Klick zum Host.

**Erfolgskriterium**: Frisches Deck → Client installieren → Host wird gefunden → Pairing per PIN → Stream startet mit automatisch perfekten Settings (1280x800@90, HEVC/AV1, HDR wenn Kette es kann). Null manuelle Konfiguration.

- [ ] Moonlight-Qt-Fork, Rebrand, Build-Pipeline (Linux + Flatpak für SteamOS)
- [ ] **Geräteprofil-Autokonfiguration**: Display-Erkennung (Auflösung, Refresh, HDR, VRR), Decoder-Probe (AV1/HEVC-HW), Netzwerk-Klasse (Ethernet/5GHz/2.4GHz) → automatische Stream-Settings mit Override
- [ ] Bekannte Geräteprofile ab Werk: Steam Deck LCD/OLED, gängige Android-TV-Boxen, 4K-TVs
- [ ] Pairing-Flow: Discovery (mDNS) + PIN, QR-Code-Option von der Host-WebUI
- [ ] „Host aufwecken"-Knopf (WoL-Magic-Packet, MAC vom letzten Pairing gemerkt)
- [ ] Zwei App-Profile: **Gaming** (Vollbild, Controller-first, Low-Latency-Preset) und **Remote Desktop** (Fenster, Maus/Tastatur-first) — eine Codebasis, zwei Launcher-Einträge

### C2 — Native Steam-Deck-Integration (Killer-Feature #2)

**Ziel**: Die Host-Bibliothek erscheint im **nativen Steam Game Mode** des Decks. Der Nutzer merkt nicht, dass er eine Streaming-App benutzt — er sieht einfach seine Spiele.

**Erfolgskriterium**: Host-Spiel XY (nicht auf dem Deck installiert) erscheint im Game Mode mit Artwork; Start → Sonnenschein-Stream im Vollbild. Spiel Z (auf beiden installiert) fragt beim Start „Lokal / Streamen" (bzw. nutzt den pro Spiel gesetzten Default). Alles ohne Steam Remote Play.

- [ ] **Host: Library-API** — `/api/library`: scannt die Steam-Bibliothek des Hosts (`steamapps/appmanifest_*.acf`), liefert AppID, Name, Installationszustand + Artwork (`librarycache`, Fallback SteamGridDB), zusätzlich die Sonnenschein-Apps-Liste (Non-Steam)
- [ ] **Deck: Sonnenschein Companion** — synct die Host-Bibliothek in die Deck-Steam-Bibliothek (`shortcuts.vdf` + Grid-Artwork), jeder Eintrag startet `sonnenschein-client --host <id> --app <appid>`
  - Ausprägung A: **Decky-Loader-Plugin** (Sync + Toggles direkt im Game Mode, Quick-Access-Menü)
  - Ausprägung B: Fallback ohne Decky — Sync-Tool im Desktop Mode (einmalig ausführen, danach automatisch via Companion-Service)
- [ ] **Lokal-oder-Stream-Logik**: Companion erkennt Spiele, die auf Deck UND Host installiert sind (AppID-Abgleich) → pro Spiel Default „Lokal" / „Stream" / „Immer fragen"
- [ ] Auto-Resync bei jeder Verbindung (neue Host-Spiele erscheinen automatisch, deinstallierte verschwinden)
- [ ] Suspend-Verhalten: Deck-Suspend pausiert den Stream, Resume verbindet neu (inkl. WoL falls Host schläft)
- [ ] **Risiken dokumentiert**: `shortcuts.vdf` ist inoffizielles Format (aber von Heroic/Lutris/NonSteamLaunchers seit Jahren stabil genutzt); Decky Loader ist Drittprojekt → Ausprägung B ist der stabile Fallback

### C3 — Android + Windows

- [ ] Android-Client (Fork moonlight-android): Fokus Android-TV + Handhelds (Ally, Odin), gleiche Autokonfiguration, WoL
- [ ] Windows-Client (aus dem Moonlight-Qt-Fork): für Laptops/Tablets, Fokus Remote Desktop + Gaming gleichwertig

### C4 — Remote-Desktop-Modus (nach Gaming-Track)

**Ziel**: Dieselbe Infrastruktur, optimiert auf Arbeit statt Latenz.

- [ ] **Multi-Display**: mehrere virtuelle Outputs pro Session (Host-Backend kann es schon konzeptionell — ein Virtual Output pro Client-Display), Client zeigt sie als getrennte Fenster/Vollbild pro Monitor
- [ ] Text-Schärfe: 4:4:4-Chroma-Modus, höhere Bitrate statt niedriger Latenz
- [ ] Clipboard-Sync (Text zuerst, Dateien optional später)
- [ ] Docking-Profile: Client merkt sich Setups (z. B. „Deck im Dock an 2 Monitoren")
- [ ] Session-Fortsetzung: Disconnect ohne Session-Ende, Reconnect nahtlos

## Crash-Reporter-Konzept

Auf Wunsch des Maintainers: ein WebUI-Feature, das Crashes sammelt und mit einem Klick als pre-filled GitHub-Issue einreichbar macht.

1. Bei Crash: `coredumpctl` + Sonnenschein-Log werden in `~/.local/state/sonnenschein/crashes/<timestamp>/` gespeichert.
2. WebUI zeigt eine Notification: "Beim letzten Stream gab es einen Crash. Möchtest du einen Bericht senden?".
3. Klick öffnet ein Modal mit dem genauen Inhalt (anonymisiert: Hostname, Username, IPs entfernt).
4. Klick "An GitHub melden" öffnet `https://github.com/Elias02345/sonnenschein/issues/new?template=crash.yml&body=...` mit pre-filled Body.
5. Maintainer bekommt strukturierte Crash-Reports als Issues, kann triagen.

Kein Server-Backend nötig, kein Datenschutz-Ärger, alles unter Nutzer-Kontrolle.

## Branch-Strategie

- `main` — stabile Releases, getaggt mit `vX.Y.Z`. Wird nur gemerged von `dev` nach Test-Bestätigung.
- `dev` — aktiver Entwicklungs-Branch. Hier landen alle PRs.
- `feature/*`, `fix/*` — kurzlebige Branches, aus `dev` ge-branched.

Update-System default: `main`. Wer `dev` will, schaltet das in der WebUI um.

## Kein-Ziel (was wir bewusst NICHT machen)

- **Kein Windows-Server-Modus** — dafür gibt es Sunshine + Apollo. Sonnenschein ist Linux. (Ein Windows-**Client** kommt im Client-Track — der Host bleibt Linux.)
- ~~**Keine eigene Client-App** — Moonlight + Artemis decken das ab.~~ **Revidiert 2026-07-11**: eigener Client mit Geräte-Autokonfiguration + nativer Steam-Deck-Integration ist jetzt der Client-Track (siehe oben). Moonlight-Kompatibilität bleibt erhalten.
- **Keine Multi-User-Tenants** — ein Sonnenschein-Account pro System reicht (Q8).
- **Keine Cloud-Hosted-Plattform** — Sonnenschein ist self-hosted, Punkt.
- **Kein Anti-Cheat-Bypass** — wenn EAC/Vanguard ein Problem haben, dann haben sie eines.

## Status

Letztes Update: 2026-07-11 (Client-Track + Boot-to-Ready aufgenommen; Host läuft End-to-End auf CachyOS + Steam Deck)
