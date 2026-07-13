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

> **Scope-Entscheidung 2026-07-12 (Maintainer):** Sonnenschein ist **KDE-only**.
> Andere Desktop-Environment-Backends (Mutter/GNOME, wlroots/Sway/Hyprland,
> Xorg-Dummy, NVIDIA-EDID-Spoof, EVDI) sind **kein Ziel mehr** — der Maintainer
> nutzt KDE Plasma, das KWin-Backend funktioniert bestätigt. Below als
> `~~durchgestrichen~~ (kein Ziel)` markiert.

- [x] `src/platform/linux/virtual_display/interface.h` — die Abstraktions-Schicht
- [x] Backend: KWin Virtual Output (kscreen-doctor + KWin-D-Bus) — ✅ live bestätigt CachyOS
- ~~Backend: AMDGPU `virtual_display=` Kernel-Param~~ (kein Ziel — KDE-only)
- ~~Backend: Mutter `--headless --virtual-monitor`~~ (kein Ziel — KDE-only)
- ~~Backend: wlroots Headless (Sway / Hyprland)~~ (kein Ziel — KDE-only)
- ~~Backend: Xorg dummy + xrandr CVT-reduced~~ (kein Ziel — KDE-only)
- ~~Backend: Xorg NVIDIA EDID-Spoof~~ (kein Ziel — KDE-only)
- ~~Backend: EVDI als Last Resort~~ (kein Ziel — KDE-only)
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
- [x] Branch-Wahl (`main` / `dev`) — Dashboard-Selector füttert den Update-Trigger; `GET /api/update-state` surft den Stand (2026-07-12)
- [x] Auto-Update als Service-Option — systemd `--user`-Timer (10 min Check, `available.json`); Auto-Apply via `AUTO_UPDATE=1` (2026-07-12)
- [x] Rollback — Auto-Rollback bei Health-Fail in `update.sh` (Backup + Vorgänger-Commit neu bauen) + manuelles `revert-update.sh` (2026-07-12)
- [ ] Migration-Framework — **N/A für das aktuelle Flat-Config-Modell** (keine DB-Schema-Migrationen; Config ist additiv). Reaktivieren falls je ein Schema dazukommt.

### Phase 7 — Tests, Performance, Dokumentation

**Ziel**: Genug Reife für eine 1.0.

**Erfolgskriterium**: Min. 4 Distros × 3 GPUs × 2 Compositoren in CI-Smoketests. <30 ms Latenz LAN. Doku decken alle Setup-Szenarien ab.

- [ ] CI: Smoketests in qemu/podman pro Distro
- [ ] Latenz-Benchmark-Suite (Frame-Trace, ENet-Probe)
- [ ] mkdocs-material Dokumentation (übernommen von Sunshine, neu strukturiert)
- [ ] Setup-Videos (DE + EN, optional)
- [ ] Troubleshooting-Knowledge-Base

### Phase 8 — 1.0 Release

**Scope-Entscheidung 2026-07-12 (Maintainer):** **Kein natives Packaging.**
Der One-Liner-Installer (`curl … | bash`) ist der einzige und bewusst gewählte
Distributionsweg — er funktioniert bestätigt auf CachyOS + Ubuntu-E2E. AUR/COPR/
PPA/Flatpak/AppImage sind **kein Ziel**. 1.0 = stabiler Installer + Doku +
Release-Tag/Announcement.

- ~~AUR `sonnenschein-bin` / `sonnenschein-git`~~ (kein Ziel — Installer reicht)
- ~~COPR (Fedora), PPA (Ubuntu)~~ (kein Ziel — Installer reicht)
- ~~Flatpak (flathub), AppImage~~ (kein Ziel — Installer reicht)
- [ ] Release-Tag `v1.0.0` + Announcement (r/linux_gaming, r/Steamdeck)

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

### Client-Track — Präzisierungen (Maintainer 2026-07-13, Runde 2)

Verbindliche Richtungsentscheidungen, die C1–C4 überlagern:
- **Repo-Konsolidierung**: Client-Code lebt **im Haupt-Repo `sonnenschein`** (als
  `client/`), **kein GitHub-Fork von Moonlight** (der alte `sonnenschein-client`-Fork
  zog alle Moonlight-Contributors/Netzwerk mit rein → wird archiviert). „Nur wir
  zwei." GPL-3 + Moonlight-Attribution bleibt (Pflicht). **Releases (gebaute Apps)
  erscheinen im Haupt-Repo** unter Releases.
- **Windows**: echte `.exe` bauen + **installierbares Programm** (Installer), klar als
  Client-Software benannt.
- **Deck Game Mode = Decky-Plugin** (tiefe native Integration, Killer-Feature) —
  bevorzugt vor Flatpak, damit ohne Desktop Mode testbar.
- **Remote-Desktop-UX = Easy + Advanced**:
  - **Easy (Default)**: alles automatisch. Beim Wählen von „Desktop"-Streaming auf
    Nicht-Gaming-Geräten → Abfrage „Remote Desktop? → Single-Monitor / Absolut?".
    **Auflösung, Skalierung, Bildwiederholrate werden von der App selbst ausgelesen
    + gesetzt — für ALLE Screens** (Multi-Monitor), beste Werte je Anwendung +
    Nutzerentscheidung automatisch. Kein manuelles Setup. Moonlights eingebaute
    RD-Features nutzen, keine doppelten Optionen.
  - **Advanced (opt-in)**: alle Toggles zurück, volle manuelle Kontrolle zum
    Ausprobieren.
- **UI**: moderner + weniger kompliziert.
- **Bekannter Gap (2026-07-13)**: Windows-App übernimmt die Client-Display-Skalierung
  noch nicht (wirkt seltsam); RD/Absolut-Toggles hatten keine sichtbare Wirkung →
  wird durch Easy/Advanced-Redesign + Auto-Resolution ersetzt.

### C1 — Client-Fundament (Linux + SteamOS)

**Ziel**: `sonnenschein-client` läuft auf Linux-Desktop und Steam Deck (Flatpak), verbindet sich mit einem Klick zum Host.

**Erfolgskriterium**: Frisches Deck → Client installieren → Host wird gefunden → Pairing per PIN → Stream startet mit automatisch perfekten Settings (1280x800@90, HEVC/AV1, HDR wenn Kette es kann). Null manuelle Konfiguration.

- [~] Moonlight-Qt-Fork, Rebrand, Build-Pipeline (Linux + Flatpak für SteamOS) — **Fundament 2026-07-13**: Upstream Moonlight-Qt v6.1 baut + läuft auf CachyOS (Deps `sdl2_ttf`/Vulkan-Headers ohne sudo lokal gelöst); Rebrand + Flatpak noch offen
- [~] **Geräteprofil-Autokonfiguration**: Display-Erkennung (Auflösung, Refresh, HDR, VRR), Decoder-Probe (AV1/HEVC-HW), Netzwerk-Klasse (Ethernet/5GHz/2.4GHz) → automatische Stream-Settings mit Override — **Kern 2026-07-13**: `AutoConfig` + CLI `detect-profile` erkennt Display + HW-Decode pro Codec → optimales Profil, live auf CachyOS verifiziert (4K60/AV1). VRR + Netzwerk-Klasse + WebUI-Override offen
- [ ] Bekannte Geräteprofile ab Werk: Steam Deck LCD/OLED, gängige Android-TV-Boxen, 4K-TVs
- [~] Pairing-Flow: Discovery (mDNS) + PIN, QR-Code-Option von der Host-WebUI — **mDNS-Discovery + PIN-Pairing 2026-07-13 live verifiziert** (voller Apollo-Handshake, Client-Zert verifiziert, App-Liste über mutual-TLS abgerufen); QR-Code-Option offen
- [ ] „Host aufwecken"-Knopf (WoL-Magic-Packet, MAC vom letzten Pairing gemerkt)
- [ ] Zwei App-Profile: **Gaming** (Vollbild, Controller-first, Low-Latency-Preset) und **Remote Desktop** (Fenster, Maus/Tastatur-first) — eine Codebasis, zwei Launcher-Einträge

### C2 — Native Steam-Deck-Integration (Killer-Feature #2)

**Ziel**: Die Host-Bibliothek erscheint im **nativen Steam Game Mode** des Decks. Der Nutzer merkt nicht, dass er eine Streaming-App benutzt — er sieht einfach seine Spiele.

**Maintainer-Vision (2026-07-13, präzisiert):** Der Client zeigt **eine einheitliche
Liste** = Desktop + Steam Big Picture + host-konfigurierte Apps (über das gepairte
Streaming-Protokoll, cert-auth) **plus alle installierten + ausführbaren Spiele**
(Steam-nativ *und* Steam-fremd) via `/api/library`. **Optional**: verfügbare, aber
nicht installierte Spiele — evtl. mit **Install-Button** (Remote-Install auf dem Host
auslösen). Die Liste wird bei **jedem Verbinden neu gemappt** (neue/aktualisierte Titel
sofort sichtbar). **Reihenfolge: Client erst auf dem PC fertigstellen + testen, dann Deck.** **Deck-Zielbild (2026-07-13):** ist der gepairte PC erreichbar, erscheinen seine Spiele direkt in der **nativen SteamOS-Game-Mode-Oberfläche** als „vom PC über Sonnenschein streamen" — realisiert über BEIDES: die native App (`sonnenschein-client`) UND ein Game-Mode-Plugin/Integration (Decky).
Auth-Konsequenz: für den nahtlosen Deck-Flow sollte `/api/library` **cert-authentifiziert**
über den gepairten Client abrufbar sein (ohne separates WebUI-Passwort) — offener Punkt.

**Erfolgskriterium**: Host-Spiel XY (nicht auf dem Deck installiert) erscheint im Game Mode mit Artwork; Start → Sonnenschein-Stream im Vollbild. Spiel Z (auf beiden installiert) fragt beim Start „Lokal / Streamen" (bzw. nutzt den pro Spiel gesetzten Default). Alles ohne Steam Remote Play.

- [x] **Host: Library-API** — `GET /api/library`: scannt alle Steam-Library-Folders (nativ + flatpak + `libraryfolders.vdf`), parst `appmanifest_*.acf`, liefert AppID/Name/Installationszustand mit Dedupe. Non-Steam-Apps bleiben auf `/api/apps` (Client merged). **2026-07-13 live auf CachyOS verifiziert: 31 echte Spiele (Haupt-Lib + `/mnt/Games`), alle installiert korrekt.** (2026-07-12 / 07-13)
- [x] **Host: Artwork** — `GET /api/library/artwork/<appid>`: streamt das lokale `librarycache`-Cover (Portrait-Grid), 404 → Client-Fallback SteamGridDB. **2026-07-13 live gegen echte Steam-Installation auf CachyOS verifiziert** + Fix für neueres Steam-Cache-Layout (Content-Hash-Subdirs + `library_capsule.jpg`; Commit `db8bcf0`) — HL2/Cyberpunk/RDR2/Witcher liefern jetzt Portrait-Cover. (2026-07-12 / 07-13)
- [~] **Host: Spiele als Stream-Apps (Ansatz a) — 2026-07-13 (`a9bbaef`, dev)**: installierte Steam-Spiele erscheinen in der cert-auth Stream-App-Liste (Desktop+Big Picture+Spiele in EINEM Grid), Launch via `steam steam://rungameid`, jpg-Boxart. Live per `moonlight list` verifiziert (26 Spiele, Runtimes gefiltert). Offen: Game-Launch+Stream real testen (Deck), Re-Scan pro Connect.
- [ ] **Deck: Sonnenschein Companion** — synct die Host-Bibliothek in die Deck-Steam-Bibliothek (`shortcuts.vdf` + Grid-Artwork), jeder Eintrag startet `sonnenschein-client --host <id> --app <appid>`
  - **Ausprägung A: Decky-Loader-Plugin (Sync + Toggles direkt im Game Mode, Quick-Access-Menü) — GEWÄHLTE RICHTUNG (Maintainer 2026-07-13): voll auf Decky für die native Game-Mode-UX mit Live-Toggles.**
  - Ausprägung B: Fallback ohne Decky — Sync-Tool im Desktop Mode (einmalig ausführen, danach automatisch via Companion-Service)
- [ ] **Lokal-oder-Stream-Logik**: Companion erkennt Spiele, die auf Deck UND Host installiert sind (AppID-Abgleich) → pro Spiel Default „Lokal" / „Stream" / „Immer fragen"
- [ ] Auto-Resync bei jeder Verbindung (neue Host-Spiele erscheinen automatisch, deinstallierte verschwinden)
- [ ] Suspend-Verhalten: Deck-Suspend pausiert den Stream, Resume verbindet neu (inkl. WoL falls Host schläft)
- [ ] **Risiken dokumentiert**: `shortcuts.vdf` ist inoffizielles Format (aber von Heroic/Lutris/NonSteamLaunchers seit Jahren stabil genutzt); Decky Loader ist Drittprojekt → Ausprägung B ist der stabile Fallback

### C3 — Android + Windows

- [ ] Android-Client (Fork moonlight-android): Fokus Android-TV + Handhelds (Ally, Odin), gleiche Autokonfiguration, WoL
- [ ] Windows-Client (aus dem Moonlight-Qt-Fork): für Laptops/Tablets, Fokus Remote Desktop + Gaming gleichwertig

### C4 — Remote-Desktop-Modus (Vision präzisiert 2026-07-13)

**Ziel**: Dieselbe Infrastruktur, optimiert auf Arbeit statt Latenz — der Host
(PC/Server) wird nativ als Computer nutzbar. **Beide Modi erzeugen client-basiert
native virtuelle Displays am Host und deaktivieren dessen physische Displays**
(baut auf dem bestehenden Virtual-Display-Backend auf, das genau das schon tut).

**Zwei Modi (Maintainer 2026-07-13):**
- **Absoluter Modus** — den ganzen Host als Setup „übernehmen": **mehrere virtuelle
  Screens** passend zu den Client-Displays, **Auto-Alignment** der virtuellen
  Outputs (mit manuellem Fallback). Für Multi-Monitor-Arbeitsplätze, die komplett
  mit dem Host arbeiten. Physische Host-Displays aus, N virtuelle rein.
- **Single-Monitor-Modus** — den Client-Rechner **nebenbei** weiternutzen: auf
  **einem wählbaren** Client-Monitor ein Host-Screen (virtuell + nativ angepasst),
  Rest des Client-Desktops bleibt frei.
- Unterschied: ganzes Setup übersteuern (absolut) vs. Client daneben weiternutzen
  (single).

**Native-Remote-Desktop-Features (alle „gut/nativ" gewünscht):**
- **Clipboard-Sync** (Text zuerst, Dateien später) — NEU, Moonlight hat keine Basis.
- **Drag & Drop** zwischen Client und Host — NEU.
- **Einzelne Fenster vom Host in den Client holen** (Seamless/RemoteApp-artig, cross-
  device, fühlt sich nativ an) — NEU, größtes Teilstück.
- **USB-Geräte-Weiterleitung** (Maintainer 2026-07-13): steckt man z. B. einen
  USB-Stick ein, **fragt der Client, ob das Gerät an den Host oder lokal soll**.
  Bei „Host" → **virtuelle Brücke** (USB/IP-artige Redirection, z. B. `usbip`
  oder gerätespezifische Kanäle) — das Gerät erscheint am Host, als wäre es dort
  gesteckt. NEU, eigener Kanal (Hotplug-Erkennung Client-seitig + Bridge zum Host).
- 4:4:4-Chroma + höhere Bitrate (Text-Schärfe), Docking-Profile, nahtlose Reconnects.

**Phasierter Plan + Machbarkeit:**
- [~] **Phase RD-0 — Modus-Fundament (Client)**: `remoteDesktopMode`-Enum
  (Off / Single-Monitor / Absolute) + Settings-UI. Single-Monitor = Fenster +
  absolute Maus (Client daneben nutzbar). *Tractable, in Arbeit 2026-07-13.*
- [ ] **Phase RD-1 — Single-Monitor E2E**: Host virtual_display=true (physische
  Displays aus, 1 virtueller Output in Client-Auflösung), Client zeigt ihn in
  einem Fenster auf dem gewählten Monitor. Nutzt bestehende Virtual-Display-Pipeline.
- [ ] **Phase RD-2 — Multi-Display / Absoluter Modus**: **großes Host-Teilstück** —
  N virtuelle Outputs pro Session (Virtual-Display-Backend von 1 auf N erweitern),
  Auto-Alignment (Client-Display-Topologie → virtuelle Output-Anordnung, manueller
  Fallback), Client zeigt N Screens (Vollbild pro Monitor). Encoder-Pipeline muss
  N Streams handhaben.
- [ ] **Phase RD-3 — Clipboard-Sync**: neuer Datenkanal (Control-Stream-Erweiterung
  oder Sidechannel) Host↔Client, Text zuerst (QClipboard ↔ Host-Clipboard via
  wl-clipboard/xclip), Dateien später. NEU von Grund auf.
- [ ] **Phase RD-4 — Drag & Drop**: baut auf Clipboard-Kanal + Datei-Transfer auf.
- [ ] **Phase RD-5 — Seamless-Windows (einzelne Fenster holen)**: **größtes Teilstück**,
  RemoteApp-artig. Host enumeriert Fenster (KWin-Scripting/Wayland), streamt ein
  einzelnes Fenster als eigenen virtuellen Output oder croppt, Client zeigt es als
  natives Fenster. Forschungs-/Prototyp-lastig.
- [ ] Docking-Profile (Client merkt Setups) + nahtlose Reconnects.

**Realität**: RD-0/RD-1 bauen auf Vorhandenem auf (tractable). RD-2..RD-5 sind
substanzielle Neu-Entwicklung (Multi-Output-Encoding, neue Datenkanäle, Fenster-
Forwarding) — mehrere Sessions, teils am Deck/Windows zu verifizieren.

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

Letztes Update: 2026-07-13 (Runde 11: Host-Library-API + Artwork live auf CachyOS gegen echte Steam-Installation verifiziert & Cache-Layout-Fix; Client-Track C1-Fundament — Moonlight-Qt baut+läuft+pairt auf CachyOS)
