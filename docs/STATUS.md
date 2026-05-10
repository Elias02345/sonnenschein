# Sonnenschein — Aktueller Projektstand (STATUS.md)

> **Lebendes Dokument.** Diese Datei wird in jeder Session nach jedem
> abgeschlossenen Schritt aktualisiert. Sie ist die einzige Quelle der
> Wahrheit für Multi-Session-Arbeit. Wenn etwas hier fehlt, weiß die
> nächste Session es nicht.

**Stand:** 2026-05-10 — **Virtual Display erstellt + Capture-Routing gefixt!**
`headless_mode=true` triggert Virtual Display Lifecycle korrekt. Capture-Routing
über KMS-Index `"0"` statt gebrochenem `map_display_name()`. Nächster Test auf CachyOS.

---

## TL;DR — Wo stehen wir gerade

- **Letzter Commit auf `dev`**: (ausstehend) — `fix(capture): route KMS capture to primary monitor for Linux virtual display`
- **Letztes erfolgreiches Build-Ziel**: WSL2 Ubuntu 24.04 (296/296 Steps grün) + CachyOS (Stream zu SteamDeck funktioniert!)
- **Erreichter Meilenstein**: Virtual Display `Sonnenschein-00E8F1E1` wird korrekt via `kscreen-doctor` erzeugt und beim Disconnect wieder entfernt. Lifecycle funktioniert perfekt.
- **Kritische offene Aufgabe**: Capture-Routing war gebrochen (`map_display_name()` gibt auf Linux `{}` zurück → KMS findet Müll-Index). Gefixt: `output_name = "0"` (primärer KMS-Monitor). Maintainer testet jetzt auf CachyOS.
- **Hauptanwendungsfall (Maintainer)**: Physische Monitore verbunden → beim Streaming deaktivieren → Virtual Display als einziger aktiver Output → Capture darauf. Headless soll auch funktionieren (für andere User).
- **Langfristig**: PipeWire-Capture (Phase 4+) für den Fall "physischer Monitor + Virtual Display gleichzeitig aktiv".

---

## Inhaltsverzeichnis

1. [Maintainer & Hardware](#1-maintainer--hardware)
2. [Repo, Branches, CI](#2-repo-branches-ci)
3. [Architektur in 5 Sätzen](#3-architektur-in-5-sätzen)
4. [Tech-Stack-Entscheidungen (Q1–Q28 + Defaults)](#4-tech-stack-entscheidungen-q1q28--defaults)
5. [Build-Setup (drei Wege)](#5-build-setup-drei-wege)
6. [Phasen-Status](#6-phasen-status)
   - [Phase 0 — Vorbereitung](#phase-0--vorbereitung)
   - [Phase 1 — Linux-Build-Pipeline](#phase-1--linux-build-pipeline)
   - [Phase 2 — Virtual-Display-Abstraktion](#phase-2--virtual-display-abstraktion)
   - [Phase 3 — Installer & Service](#phase-3--installer--service)
   - [Phase 4 — HDR & AV1](#phase-4--hdr--av1)
   - [Phase 5 — WebUI v1 (PrimeVue 4)](#phase-5--webui-v1-primevue-4)
   - [Phase 6 — Update-System](#phase-6--update-system)
   - [Phase 7 — Tests, Performance, Doku](#phase-7--tests-performance-doku)
   - [Phase 8 — 1.0 Release](#phase-8--10-release)
7. [Modul-Landkarte (Wo lebt was)](#7-modul-landkarte-wo-lebt-was)
8. [Submodules](#8-submodules)
9. [Bekannte Probleme & Workarounds](#9-bekannte-probleme--workarounds)
10. [Letzte Commits chronologisch](#10-letzte-commits-chronologisch)
11. [Datei-Inventar (was wir geschrieben haben)](#11-datei-inventar-was-wir-geschrieben-haben)
12. [Was als nächstes — konkrete Schritte](#12-was-als-nächstes--konkrete-schritte)
13. [Update-Konventionen für diese Datei](#13-update-konventionen-für-diese-datei)

---

## 1. Maintainer & Hardware

| Feld | Wert |
|---|---|
| Name | Elias Kanakidis |
| GitHub | [@Elias02345](https://github.com/Elias02345) |
| Email | elias-kanakidis@gmx.de |
| Sprache | **Deutsch** (mit Maintainer immer auf Deutsch antworten) |
| Dev-Workstation | Windows 11 + WSL2 Ubuntu 24.04 + Cursor/VSCode |
| **Test-Target** (CachyOS-Gaming-PC) | CachyOS rolling, Plasma 6.6.4 Wayland, KWin 6.6.4, NVIDIA RTX 3070, NVIDIA Driver 595.71.05, CUDA 13.2.1, libva 2.23.0, FFmpeg 8.1.1, Boost 1.91 (modular install — `system` component fehlt im default boost-Paket), CMake 4.3.2, GCC 16.1.1, Git 2.54.0, Node ≥20 |
| Login-Shell | `fish` (relevant für Heredocs/Quoting in Befehlen) |

**Hardware-Implikation**: NVIDIA-Pfad ist Phase-2-Hauptziel. RTX 3070 + Driver 595 hat alles für Wayland-HDR (`VK_EXT_hdr_metadata` ab 580.94.11). Aber der Maintainer will explizit: **alle drei GPU-Vendoren (NVIDIA, AMD, Intel) müssen voll funktionieren**, mit automatischer Backend-Auswahl + Multi-GPU-Manual-Override in der WebUI.

---

## 2. Repo, Branches, CI

- **Repo**: <https://github.com/Elias02345/sonnenschein> (public, GPL-3.0, 12 Topics, Discussions an, Wiki aus, Default-Branch `main`)
- **Branches**:
  - `main` — stabile Releases. Aktuell beim Initial-Commit `235920b`. Bleibt da bis Phase-7-Done + 1.0-Release.
  - `dev` — aktive Entwicklung. **Hier passiert alles.** PRs sollten gegen `dev` gerichtet sein, nicht gegen `main`.
  - keine kurzlebigen Feature-Branches aktuell aktiv.
- **GitHub-Konfiguration**:
  - Topics: `linux gaming streaming moonlight sunshine apollo gamestream wayland kde gnome hdr virtual-display`
  - Description: "Linux-native, headless Moonlight streaming backend with auto-matching virtual displays. Sunshine/Apollo-derived."
  - Discussions enabled.
- **CI** (`.github/workflows/`):
  - `lint.yml` — shellcheck + prettier + clang-format auf Push/PR.
  - `build-linux.yml` — Build-Matrix für Arch / Ubuntu 24.04 / Fedora 41. Aktuell `continue-on-error: true` während Phasen 1+2 stabilisieren — Flag entfernen wenn Distro 100 % grün baut. CI-Logs unter <https://github.com/Elias02345/sonnenschein/actions>.
  - Submodules in CI: aktuell `submodules: false` — third-party muss in CI separat geholt werden, oder Submodules wieder einschalten sobald Build-Pipeline auf allen Distros stabil ist.

---

## 3. Architektur in 5 Sätzen

```
            ┌──────────────────────────────────────┐
 Moonlight  │  Pairing → ResExchange → RTSP/UDP    │  Sonnenschein-Server (C++23)
   Client → │  Capabilities (HDR? AV1? Auflösung?) │  ├─ Virtual-Display-Abstraktion (NEU)
            └──────────────────────────────────────┘  │   src/platform/linux/virtual_display/
                                                      │   (Mutter / KWin / AMDGPU /
                                                      │    wlroots / Xorg / EVDI)
                                                      ├─ Capture (PipeWire-Portal /
                                                      │           KMS direct)
                                                      ├─ Encoder (NVENC / VAAPI /
                                                      │           Vulkan-Video)
                                                      ├─ Audio (PipeWire / PulseAudio)
                                                      ├─ Input (inputtino / uinput)
                                                      └─ WebUI (Vue 3 + PrimeVue 4)
                                                          ├─ Setup-Wizard
                                                          ├─ Diagnose-Tab
                                                          ├─ Update-Manager
                                                          └─ Crash-Reporter → GitHub
```

Der **virtual_display**-Layer ist Sonnenscheins eigentliche Innovation. Sechs Backend-Implementierungen (in `src/platform/linux/virtual_display/backends/`), zur Laufzeit gewählt via `factory::select_backend(env, cfg)`. Wir leihen Apollos C++-Stack komplett aus, hängen die Linux-Implementierung in den Windows-`#ifdef _WIN32`-Pfad als `#elif __linux__` ein.

---

## 4. Tech-Stack-Entscheidungen (Q1–Q28 + Defaults)

Vom Maintainer in der initialen Session bestätigt. **Nicht hinterfragen** — wenn du einen davon zur Diskussion stellen willst, frag explizit.

### A — Scope & Plattform
1. **Mindest-Distros**: Ubuntu 22.04+, Debian 12+, Fedora 40+, Arch (rolling) ✅
2. **X11-Support**: ja, als Fallback. Wayland-first.
3. **NVIDIA-Treiber**: neueste (≥ 580.94.11 für Wayland HDR; Maintainers Driver: 595.71.05).
4. **Compositors First-Class**: KDE Plasma (Hauptfokus) + GNOME + Sway + Hyprland + Cosmic + niri (best-effort).
5. **HDR auf NVIDIA**: ja zur 1.0, mit SDR-Fallback wenn nicht verfügbar.

### B — User-Erlebnis
6. **Installer-Frontend**: Text-UI mit interaktiven Elementen (whiptail/dialog).
7. **Auto-Start als Service**: Hintergrund-Dienst, kein Terminal nötig nach Login.
8. **Multi-User**: ein Account pro System (für 1.0).
9. **Auth**: User/Passwort + Login-Screen (Browser-Pass-Save-fähig).
10. **Sprachen**: DE + EN zur 1.0, mehr via Crowdin nach Release.

### C — Technische Detail-Entscheidungen
11. **Fork-Strategie**: Apollo als Basis, Sunshine-Upstream-Sync regelmäßig.
12. **Audio**: PipeWire Default + PulseAudio Fallback.
13. **Gamescope**: opt-in pro App (WebUI-Toggle).
14. **WebUI-Framework**: **PrimeVue 4** (modern, hochwertige Animationen, gamer-tauglich).
15. **Theme**: Dunkelmodus-Default, optional Hell + System-Default-Folger.
16. **Mobile-WebUI**: ja, responsive bis 360 px.

### D — Operational
17. **Update-Modell**: GitHub-Releases, automatisch + auf Knopfdruck.
18. **Telemetrie**: opt-in Crash-Reporter, sammelt anonymisierten Stacktrace + System-Info, öffnet pre-filled GitHub-Issue auf [Elias02345/sonnenschein/issues](https://github.com/Elias02345/sonnenschein/issues/new?template=crash.yml). Kein Server-Backend.
19. **Logging**: journald als Primary, JSON-strukturiert auch unter `~/.local/state/sonnenschein/logs/` mit 7-Tage-Rotation. Live-Log-Tab in WebUI streamt journalctl.
20. **Migrations**: pro Major-Version Up-Script + Backup vor jedem Lauf, Rollback-Befehl.

### E — Branding & Repo
21. **Branding**: modern, gamer-like (Logo TBD — aktuell Apollo-Logos als Platzhalter).
22. **Tagline**: _"Linux-Game-Streaming, das einfach funktioniert."_
23. **Repo-Layout**: Monorepo (server + WebUI + installer + docs in einem Repo).
24. **Branch-Strategie**: `main` stable + `dev` development. Update default = `main`. Wenn `main` neuer als `dev` → automatisch zu `main`.
25. **Release-Kadenz**: rolling.

### F — Hardware & Performance
26. **Steam + Proton**: ja, der Standard-Pfad für Linux-Gaming. Anti-Cheat (EAC/Vanguard) ist kein Ziel.
27. **Performance-Ziel**: <30 ms LAN-Latenz, hohe Qualität, adaptiv ans Netzwerk.
28. **Hardware-Mindeststandard**: so weit wie möglich. RTX 3070 muss explizit perfekt funktionieren (Maintainer-Hardware). Codec-Reihenfolge: AV1 wenn supported → HEVC → H.264.

### Zusatz vom Maintainer (nicht durchnummeriert)
- **Uninstaller**: ja, sauberes Entfernen aller Komponenten.
- **GPU-Auswahl**: automatisch per Detection, manueller Override in WebUI (Multi-GPU-Systeme).

---

## 5. Build-Setup (drei Wege)

### A) GitHub Actions CI
- Pro Push auf `main` oder `dev` und auf jedem PR.
- Container für Arch (latest), Ubuntu 24.04, Fedora 41.
- Aktuell **permissiv** (`continue-on-error: true`) während Phase 1+2 stabilisiert. Wenn Phase 7 erreicht ist, dieses Flag entfernen und CI hart machen.
- Submodules: derzeit `submodules: false` (siehe Issue im build-linux.yml — wir holen sie nicht in CI, weil build-deps mit 1.1 GB die Action-Quota sprengen würde).

### B) WSL2 Ubuntu 24.04 (auf Maintainers Windows-PC)
- Build-Verzeichnis: **`/root/snsbuild`** (NICHT `/tmp/snsbuild` — `/tmp` wird beim WSL-Restart gewipt!).
- Source bleibt unter `/mnt/c/Users/cooki/Documents/ClaudeCode/sonnenschein` (Windows-mounted).
- Kommando aus PowerShell:
  ```powershell
  wsl -d Ubuntu --user root -- bash -c "cd /root/snsbuild && cmake -S /mnt/c/Users/cooki/Documents/ClaudeCode/sonnenschein -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DSUNSHINE_BUILD_DOCS=OFF -DSUNSHINE_BUILD_HOMEBREW=OFF -DSUNSHINE_BUILD_FLATPAK=OFF -DSUNSHINE_ENABLE_CUDA=OFF && cmake --build . --target sunshine -j8"
  ```
- **Wichtige WSL-Quirks**:
  - `sudo` braucht Passwort; nutze `wsl --user root` direkt.
  - `/tmp` überlebt keinen WSL-Restart — Build-Dir auf `/root` legen.
  - libva 2.20 (Ubuntu Stock) ist zu alt; wir bauten libva 2.22 from source einmalig (Anleitung in `docs/building.md`). Nicht jedes Build wiederholen.
  - Build-Performance: /mnt/c-IO ist langsam (NTFS-via-9P). Cold configure ~6 min, Build ~10-15 min auf 8 Cores. Inkrementell ~3 min nach Code-Änderung.

### C) CachyOS-Rechner (Maintainers Test-Hardware)
- **Test-Target**, das einzige System, auf dem wir den realen End-to-End-Stream testen.
- Pfad zum Repo: `~/sonnenschein`.
- **Build geht über SSH** (kein Display nötig). **Stream-Test geht NICHT über SSH** (kscreen-doctor braucht WAYLAND_DISPLAY + DBus-Session-Bus, deshalb eine Konsole **innerhalb der Plasma-Sitzung** öffnen).
- Build-Sequenz:
  ```bash
  cd ~/sonnenschein
  git fetch && git checkout dev && git pull
  git submodule update --init --recursive   # ~5-10 min Erstmal
  rm -rf build && mkdir build && cd build
  cmake -G Ninja -S .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=OFF \
      -DSUNSHINE_BUILD_DOCS=OFF \
      -DSUNSHINE_BUILD_FLATPAK=OFF
  cmake --build . -j$(nproc)
  ```
- **Ergebnis**: `~/sonnenschein/build/sunshine-0.0.0` (Binary) + `~/sonnenschein/build/sunshine` (Symlink).

---

## 6. Phasen-Status

Legende: ✅ done · 🟡 in_progress · 🔴 blocked · ⏸ pending

### Phase 0 — Vorbereitung ✅

**Ziel**: Repo strukturiert, Lizenz, Doku-Skelett, Community-Files, CI-Skelett.

**Erfolg**: GitHub-Repo public, Branches `main` + `dev`, README/ROADMAP/CONTRIBUTING/SECURITY/Issue-Templates/CODE_OF_CONDUCT alle drin, GitHub Actions skelettiert.

| Aufgabe | Status | Commit | Notes |
|---|---|---|---|
| Apollo-Codebasis als Basis importieren | ✅ | `235920b` | 479 Dateien, 99 k Zeilen, Squash-Import (keine Apollo-Historie) |
| `_reference/` mit Sunshine + docker-steam-headless als Referenz | ✅ | gitignored | im Working-Dir, nicht im Repo |
| LICENSE (GPL-3.0) + NOTICE | ✅ | `235920b` | Attribution für Sunshine + Apollo + docker-steam-headless |
| README.md (DE/EN, modern, Architektur-Diagramm, Status-Tabelle) | ✅ | `235920b` | |
| docs/ROADMAP.md (8 Phasen, Crash-Reporter-Konzept) | ✅ | `235920b` | |
| .github/CONTRIBUTING.md, CODE_OF_CONDUCT.md, SECURITY.md, PULL_REQUEST_TEMPLATE.md | ✅ | `235920b` | |
| Issue-Templates (bug, feature, **crash** für WebUI-Reporter pre-fill, config) | ✅ | `235920b` | crash.yml hat anonymized-Checkbox |
| .github/workflows/lint.yml | ✅ | `235920b` | shellcheck + prettier + clang-format |
| .github/workflows/build-linux.yml | ✅ | `235920b` | Arch/Ubuntu/Fedora-Matrix, continue-on-error: true |
| package.json: name → "sonnenschein" | ✅ | `235920b` | Vue 3.5.22, Vite 6.3.6 |
| .gitignore erweitert (`_reference/` etc.) | ✅ | `235920b` | |
| .gitmodules ursprünglich geparkt (Phase 0), in Phase 1 wiederhergestellt | ✅ | `a95f2ee` | siehe Phase 1 |
| Initial-Commit + Push zu `dev` und `main` | ✅ | `235920b` | |
| GitHub Repo-Settings (Topics, Description, Default-Branch, Discussions) | ✅ | via gh CLI | 12 Topics, default `main`, Discussions an, Wiki aus |

### Phase 1 — Linux-Build-Pipeline ✅

**Ziel**: Apollo-Codebasis baut auf Linux ohne harte Patches, sunshine-Binary läuft, WebUI-Bundle wird produziert.

**Erfolgskriterium erreicht**: 270/270 Build-Steps grün auf WSL2 Ubuntu 24.04. Binary 33 MB, `--help` zeigt korrektes Usage. WebUI-Bundle in `build/assets/web/`.

| Aufgabe | Status | Commit | Notes |
|---|---|---|---|
| 17 Submodules eingebunden via `_reference/add_submodules.sh` | ✅ | `a95f2ee` | inkl. moonlight-common-c/enet rekursiv (wegen nested submodule), tray gepinnt auf `7936cb35` (Pre-Qt-Migration) |
| Build-Deps in WSL2 installiert | ✅ | (manuell) | Liste in `docs/building.md` |
| **libva 2.22 from source** in WSL2 | ✅ | (manuell) | Ubuntu 24.04 Stock libva 2.20 fehlt `vaMapBuffer2`, das build-deps' FFmpeg braucht |
| Node 20 in WSL2 (statt Node 18) | ✅ | (manuell) | Vite 6 + @vitejs/plugin-vue 6 brauchen Node ≥20.19 (`crypto.hash`) |
| WebUI-Build (npm install + npm run build) | ✅ | (zur Verifikation) | 17 Module via Vite, ~2 min |
| Fix: `tray_linux.c` (nicht `.cpp`) — Apollo erwartete .c, master-tray hat zu Qt+`.cpp` migriert; deshalb tray-Submodule auf `7936cb35` (letzter GTK-Commit) gepinnt | ✅ | `a95f2ee` | |
| cmake configure grün | ✅ | (verifikation) | `-DSUNSHINE_ENABLE_CUDA=OFF` für WSL (kein NVIDIA) |
| cmake build grün, sunshine-Binary kompiliert | ✅ | (verifikation) | 33 MB ELF, läuft mit `--help` |
| docs/building.md komplett neu für Sonnenschein | ✅ | `539d3a5` | Per-Distro-Lists (Arch/Ubuntu/Fedora/openSUSE), libva-2.22-Anleitung, WSL2-Workflow, Troubleshooting-Sektion |
| Phase 1.6: CMake `project(Apollo)` → `project(Sonnenschein)` Rebrand | ⏸ | — | **bewusst aufgeschoben bis nach Phase 2** — riskant, betrifft viele Pfade/Configs/Service-Files. In den meisten Logs heißt das Binary noch "Apollo". |

### Phase 2 — Virtual-Display-Abstraktion 🟡

**Ziel**: Apollos Killer-Feature (Auto-Match Virtual Display beim Pairing) auf Linux portieren. Multi-Backend-Abstraktion mit automatischer Auswahl + Manual-Override.

#### Phase 2A — Foundation ✅ ([`7cdd3f0`](https://github.com/Elias02345/sonnenschein/commit/7cdd3f0))

| Datei | LOC | Inhalt |
|---|---|---|
| `src/platform/linux/virtual_display/types.h` | 135 | Enums (DisplayServer, Compositor, GpuVendor) + GpuInfo / Environment / CreateRequest / Handle |
| `src/platform/linux/virtual_display/interface.h` | 59 | `IBackend` Vertrag (name, display_name, available, create, destroy, destroy_all) |
| `src/platform/linux/virtual_display/detection.h+cpp` | 29+431 | `detect()` Environment-Detection: WAYLAND_DISPLAY/DISPLAY/XDG_*/`/sys/class/drm/card*`/`/proc/driver/nvidia/version`/lspci → Environment-Struct |
| `src/platform/linux/virtual_display/factory.h+cpp` | 52+94 | `select_backend(env, cfg)` Auto-Selection mit `preferred_backend`-Override aus sonnenschein.conf |
| `src/platform/linux/virtual_display/backends/all.h` | 53 | `make_*()` Factory-Function-Deklarationen |
| 7 Backend-Stub-Dateien (`backends/{kwin_wayland, mutter_headless, wlroots_headless, xorg_nvidia, xorg_dummy, amdgpu_param, evdi}.cpp`) | je ~50 | Jeder Stub: korrektes `available()` + leeres `create()/destroy()` + ausführlicher `Plan:`-Doc-Block für die spätere Implementierung |
| `src/platform/linux/virtual_display/README.md` | ~150 | Architektur, File-Map, Status-Tabelle, Wiring-Point für Phase 2C |
| `cmake/compile_definitions/linux.cmake` | +14 | Alle 14 Files in `PLATFORM_TARGET_FILES` aufgenommen |

**Build-Verifikation**: 270/270 Steps grün, Binary verlinkt mit 135 `sonnenschein::vdisplay`-Symbolen (via `nm`).

#### Phase 2B — KWin-Wayland Backend ✅ ([`509e87a`](https://github.com/Elias02345/sonnenschein/commit/509e87a))

| Datei | LOC | Inhalt |
|---|---|---|
| `src/platform/linux/virtual_display/subprocess.h+cpp` | NEU, ~210 | `run_shell(cmd, desc)` (popen-basiert) + `run_args(argv, desc)` (fork+execvp). Beide capturen stdout+stderr (≤64 KB), decoden wstatus, BOOST_LOG-Diagnostik. |
| `src/platform/linux/virtual_display/backends/kwin_wayland.cpp` | REWRITE, ~250 | `KwinWaylandBackend`: ruft `kscreen-doctor add-virtual-output NAME WIDTH HEIGHT`, dann `kscreen-doctor output.NAME.mode.WxH@HZ` für genaue Refresh-Rate, dann `output.NAME.hdr.true` wenn Client HDR-fähig. Stable per-client display-naming `Sonnenschein-<8-hex-chars-aus-client-uid>`. Thread-safe `std::map<display_id, ActiveDisplay>` für Cleanup. "already exists"-Recovery durch remove + re-add. |
| `cmake/compile_definitions/linux.cmake` | +2 | subprocess.h/.cpp in PLATFORM_TARGET_FILES |

**Build-Verifikation**: 10/10 incremental steps grün, `nm` zeigt `make_kwin_wayland`, `run_shell`, `run_args` im Binary.

**Zielsystem**: Plasma 6.4+ (`add-virtual-output` Subcommand). User hat Plasma 6.6.4 → kompatibel. Falls Plasma <6.4: KWin-Plugin-Backend nötig (Phase 2B.5, noch nicht implementiert).

#### Phase 2C — process.cpp Wiring ✅ ([`72cf5c1`](https://github.com/Elias02345/sonnenschein/commit/72cf5c1))

Apollo's `process.cpp` hatte einen `#ifdef _WIN32`-Block für Virtual Display (SudoVDA / `VDISPLAY::createVirtualDisplay`). Wir haben einen Sibling `#elif __linux__`-Block hinzugefügt.

| Datei | Änderung |
|---|---|
| `src/process.h` | `#elif __linux__ #include "platform/linux/virtual_display/interface.h"` + zwei neue private Member unter `#ifdef __linux__`: `std::unique_ptr<sonnenschein::vdisplay::IBackend> _vdisplay_backend;` und `std::optional<sonnenschein::vdisplay::Handle> _vdisplay_handle;` |
| `src/process.cpp` | Zwei `#elif __linux__`-Blöcke: einer in `execute()` (~70 LOC, ruft detect → select_backend → create), einer in `terminate()` (~10 LOC, ruft `backend->destroy(handle->display_id)`). Gleichzeitig Restrukturierung des bestehenden `#ifdef _WIN32 ... #else ... #endif` zu `#ifdef _WIN32 ... #elif __linux__ ... #else ... #endif`, sodass `display_device::configure_display()` + `reset_persistence()` für beide Plattformen mit Virtual Display laufen. macOS-Pfad bleibt unverändert. |

**Build-Verifikation**: 3/3 incremental Steps grün auf WSL2, Binary funktioniert.

**Effekt**: Wenn Sonnenschein auf KDE Plasma 6+ Wayland läuft und ein Moonlight-Client paart, der virtuelle Display angefordert hat (oder `headless_mode` global ist, oder die App-Config `virtual_display=true` hat), wird ein KWin Virtual Output mit der genauen WxH@HZ + HDR des Clients erzeugt — wie auf Windows mit SudoVDA, aber Linux-nativ.

#### Phase 2D — End-to-End Test auf CachyOS 🟡 (in Arbeit, blockiert)

**Ziel**: Maintainer testet das Sonnenschein-Binary auf seinem RTX-3070 + Plasma-6.6.4-Wayland-System mit einem realen Moonlight-Client.

**Build-Fixes** (iterativ gelöst):

1. **Boost ALIAS-Target-Kollision** (gefixt in [`b6f263e`](https://github.com/Elias02345/sonnenschein/commit/b6f263e)) — Pre-Flight-Check + FetchContent-Fallback.
2. **CUDA nicht gefunden auf Arch/CachyOS** (gefixt in [`b6f263e`](https://github.com/Elias02345/sonnenschein/commit/b6f263e)) — `/opt/cuda/bin` Auto-Detection.
3. **CUDA 13.2 inkompatibel mit GCC 16.1.1** (gefixt in [`5e1b1cf`](https://github.com/Elias02345/sonnenschein/commit/5e1b1cf)) — GCC-Versionsprüfung vor `check_language(CUDA)`, graceful skip.
4. **CUDA_FAIL_ON_MISSING feuerte trotz intentionalem Skip** (gefixt in [`8126f26`](https://github.com/Elias02345/sonnenschein/commit/8126f26)) — Guard-Bedingung erweitert.

**Erster E2E-Test (2026-05-10) — Ergebnis: STREAMING FUNKTIONIERT! ✅**

- Binary erfordert `sudo setcap cap_sys_admin+p` (siehe §9.10) → danach fehlerfrei.
- WebUI erreichbar auf `https://localhost:47990` → User+Passwort gesetzt.
- **SteamDeck via Moonlight erfolgreich gepairt und gestreamt!** Bildschirmspiegelung des Hauptmonitors (HDMI-A-1, Samsung 4K).
- Encoder: nicht im Log welcher griff, aber Stream lief → mindestens einer (wahrscheinlich VAAPI oder Software) funktionierte nach `cap_sys_admin`.
- **Virtual Display wurde NICHT erzeugt** — das ist **erwartet und korrekt**: der Code-Pfad in `process.cpp:346-350` triggert nur wenn:
  - `config::video.headless_mode == true` (Config-Datei: `headless_mode = true`), ODER
  - `launch_session->virtual_display == true` (Client-Request), ODER
  - `_app.virtual_display == true` (per-App-Setting in WebUI/apps.json)
  - Default für alle drei: `false`.

**Nächster Schritt**: Maintainer testet den Capture-Routing-Fix auf CachyOS. Erwartung: kein `Couldn't find monitor` Error mehr, Stream zeigt den Desktop.

**Hauptanwendungsfall (geklärt 2026-05-10)**: Physische Monitore sind verbunden, sollen sich beim Streaming deaktivieren und Platz für einen virtuellen Monitor machen. Virtual Display wird dann der einzige aktive Output → KMS-Index 0 zeigt darauf. Headless (kein physischer Monitor) soll ebenfalls funktionieren (andere User). Langfristig (Phase 4+): PipeWire-Capture für den Fall, dass physische + virtuelle Monitore gleichzeitig aktiv sind.

### Phase 2E — Capture-Routing Fix

**Problem**: `map_display_name()` gibt auf Linux `{}` zurück (Windows-only `sm_instance`). `kmsgrab.cpp` parst den leeren String als Integer → `1105514439` (Müll) → `Couldn't find monitor [1105514439]` → Fallback auf HDMI-A-1 → Screen Mirroring statt Virtual Display.

**Fix**: In `process.cpp` Zeile 392-397, `map_display_name()` durch `output_name = "0"` ersetzt. KMS-Index `0` = primärer Monitor. Im Headless-Modus ist der Virtual Display der einzige Output → KMS-Index 0 zeigt genau darauf.

### Phase 3 — Installer & Service ⏸

**Ziel**: Ein Bash-Skript, distroübergreifend, robust, klare Fehlermeldungen. Auf vier frischen VMs (Arch, Ubuntu, Fedora, openSUSE) macht `curl ... | bash` jeweils eine funktionierende Sonnenschein-Instanz.

**Geplante Struktur** (aus Original-Plan):
```
installer/
├── install.sh              # Entry, Distro-Detect, Sudo-Check, UI-Mode-Wahl
├── lib/
│   ├── distro.sh           # detect_distro() → arch | fedora | debian | ubuntu | opensuse
│   ├── gpu.sh              # detect_gpu() → nvidia | amd | intel | hybrid
│   ├── compositor.sh       # detect_compositor() → kwin | mutter | sway | hyprland | xorg
│   ├── packages.sh         # install_packages() — distro-spezifisch
│   ├── service.sh          # systemd-Unit-Setup (--user mode)
│   ├── permissions.sh      # udev-Rules, Group-Memberships, capabilities (cap_sys_admin für KMS)
│   └── ui.sh               # whiptail/dialog frontend, Fortschrittsbalken
├── packages/
│   ├── arch.list
│   ├── debian.list
│   ├── fedora.list
│   └── opensuse.list
├── post-install.sh         # ersten Pairing-PIN ausgeben, WebUI-URL
├── update.sh               # GitHub-Releases-API, Versionsvergleich, tarball-download, symlink-switch
└── uninstall.sh            # alles spurlos entfernen
```

**Robustheits-Pflichten**:
- Jeder Schritt idempotent
- Jeder Schritt in `set -euo pipefail` mit eigenem trap-Handler
- Logfile: `/var/log/sonnenschein-install.log`
- Jede Fehlermeldung: Kontext + Vorschlag zur manuellen Korrektur

**Service-Layout**:
- `sonnenschein-server.service` (system oder user) — der Streaming-Server. Braucht `/dev/uinput`, `/dev/dri/*`, ggf. `cap_sys_admin` für KMS-Capture.
- Bei Wayland-Compositor-Auto-Match-Virtual-Display: zwingend `systemctl --user`-Modus (DBus-Session-Bus + WAYLAND_DISPLAY).

**Distro-spezifische Pakete**:
- Arch/CachyOS: `nvidia-open-dkms nvidia-utils libva-nvidia-driver` (NVIDIA), `vulkan-radeon` (AMD), `vulkan-intel intel-media-driver` (Intel) — siehe `docs/building.md`.
- Fedora: `akmod-nvidia xorg-x11-drv-nvidia-cuda` (NVIDIA über RPMFusion).
- Ubuntu/Debian: `nvidia-driver-580-open libnvidia-encode-580` (NVIDIA über HWE/PPA).

### Phase 4 — HDR & AV1 ⏸

**Ziel**: HDR10 funktioniert auf KDE Plasma 6 Wayland mit AMD-RDNA3 + NVIDIA-Driver-580+. AV1 wird priorisiert, wenn Client+GPU es können.

**Sub-Tasks**:
- Capability-Negotiation aus Apollo (`nvhttp.cpp:960-989`) übernehmen — die `serverinfo`-Flags (`SCM_HEVC_MAIN10`, `SCM_AV1_MAIN10`).
- VAAPI HDR-Encoder-Pfad (Sunshine ist hier nur Stub — `video.cpp:492-512`).
- NVENC HDR-Encoder-Pfad (Apollos `nvenc/nvenc_utils.cpp:53-90` adaptieren — `nvenc_colorspace_from_sunshine_colorspace()`).
- Vulkan Video Encode (experimental).
- Compositor-Side: HDR-Output-Aktivierung pro Stream (KWin `output.NAME.hdr.true`, Mutter wenn protokoll-stabil).
- Driver-Versionsprüfung im Installer + Warnung bei NVIDIA <580.
- AV1-Preference-Schalter in WebUI.

### Phase 5 — WebUI v1 (PrimeVue 4) ⏸

**Ziel**: Modernes, gamer-taugliches Frontend. Ein Nutzer ohne Vorwissen kann nach Installer-Run die WebUI öffnen, in <5 min ein erstes Pairing durchführen, ein Spiel streamen — ohne Terminal anzufassen.

**Stack**:
- Vue 3 + Vite (bleibt aus Apollo)
- **PrimeVue 4** (Wechsel von Apollos Bootstrap)
- vue-i18n für DE + EN
- Dunkelmodus-Default + Hell + System-Default-Folger
- Responsive bis 360 px Mobile

**Komponenten**:
- Login-Screen (Browser-Pass-Save-fähig)
- Setup-Wizard: Pairing-PIN, GPU-Bestätigung, Compositor-Test, erstes Spiel hinzufügen
- Dashboard: Live-GPU-Auslastung, aktuelle Streams, Display-Zustand
- Apps-Verwaltung (wie Sunshine + Per-App-Gamescope-Toggle + Per-App-HDR-Override)
- **Virtual-Display-Tab**: Backend-Wahl (auto / kwin_wayland / mutter / ...), GPU-Pin (auto / 0000:01:00.0), Test-Knopf
- Geräte-Verwaltung (gepairte Clients, Permissions)
- Diagnose-Tab: Compositor, GPU, Treiber, Kernel, Mesa, HDR-Capability
- Live-Log-Tab (journald-Stream)
- Update-Manager
- **Crash-Reporter**: anonymisierter Bundle, "An GitHub melden"-Knopf öffnet pre-filled Issue-Form

### Phase 6 — Update-System ⏸

**Sub-Tasks**:
- `update.sh` (CLI): GitHub-Releases-API + Versionsvergleich + Tarball-Download + Symlink-Switch + Service-Restart.
- WebUI-Update-Knopf.
- Branch-Wahl `main` vs `dev`. Wenn `main` neuer als `dev` → automatisch zu `main`.
- Migration-Framework: pro Major-Version Up-Script + Backups vor jedem Lauf, Rollback-Befehl.
- Auto-Update als Service-Option (timer-basiert) oder On-Demand.

### Phase 7 — Tests, Performance, Doku ⏸

**Erfolgskriterium**: ≥4 Distros × 3 GPUs × 2 Compositoren in CI-Smoketests, <30 ms Latenz LAN, vollständige Doku.

**Sub-Tasks**:
- CI-Smoketests in qemu/podman pro Distro.
- Latenz-Benchmark-Suite (Frame-Trace, ENet-Probe).
- mkdocs-material Dokumentation (Sunshines Setup neu strukturiert).
- Optional: Setup-Videos.
- Troubleshooting-Knowledge-Base.

### Phase 8 — 1.0 Release ⏸

**Sub-Tasks**:
- AUR `sonnenschein-bin` und `sonnenschein-git`.
- COPR (Fedora).
- PPA (Ubuntu).
- Flatpak (flathub-Submission).
- AppImage als Fallback.
- Release-Announcement (r/linux_gaming, r/Steamdeck).

---

## 7. Modul-Landkarte (Wo lebt was)

```
sonnenschein/
├── CLAUDE.md                          # Pflichtlektüre für Claude-Code-Sessions
├── README.md                          # Public-Facing, DE/EN
├── LICENSE / NOTICE                   # GPL-3.0 + Attribution
├── CMakeLists.txt                     # project(Apollo) — Phase 1.6 wird zu Sonnenschein
├── package.json                       # name "sonnenschein", Vue+Vite Deps
├── .github/                           # Community-Files + CI
│   ├── CONTRIBUTING.md, CODE_OF_CONDUCT.md, SECURITY.md
│   ├── PULL_REQUEST_TEMPLATE.md
│   ├── ISSUE_TEMPLATE/{bug,feature,crash,config}.yml
│   ├── workflows/{lint,build-linux}.yml
│   └── copilot-instructions.md
├── cmake/                             # Apollo-CMake-System (modular)
│   ├── compile_definitions/linux.cmake     # ⭐ Sonnenschein-Patches: virtual_display sources, libva, CUDA-/opt/cuda detection, Boost-pre-flight via Boost_Sunshine.cmake
│   └── dependencies/Boost_Sunshine.cmake   # ⭐ Sonnenschein-Patches: pre-flight check + FetchContent fallback
├── docs/
│   ├── ROADMAP.md                     # 8-Phasen Vision
│   ├── STATUS.md                      # ⭐ DIESE DATEI: lebendiger Stand
│   ├── SESSION_PROMPT.md              # ⭐ Vorlage für neue Claude-Sessions
│   ├── building.md                    # Build pro Distro
│   └── (Apollo-Docs: api, configuration, getting_started, etc.)
├── src/                               # C++ Core (Apollo-Fork)
│   ├── platform/
│   │   ├── common.h
│   │   ├── linux/
│   │   │   ├── audio.cpp              # PulseAudio
│   │   │   ├── kmsgrab.cpp            # KMS direct capture
│   │   │   ├── wayland.cpp, wlgrab.cpp # wlroots screencopy
│   │   │   ├── x11grab.cpp
│   │   │   ├── vaapi.cpp              # VAAPI encoder
│   │   │   ├── input/inputtino_*      # uinput-basiert
│   │   │   ├── publish.cpp, misc.cpp, graphics.cpp
│   │   │   └── ⭐ virtual_display/    # ⭐ SONNENSCHEINS NEUE EBENE
│   │   │       ├── types.h, interface.h
│   │   │       ├── detection.h+cpp
│   │   │       ├── factory.h+cpp
│   │   │       ├── subprocess.h+cpp
│   │   │       ├── README.md
│   │   │       └── backends/
│   │   │           ├── all.h
│   │   │           ├── kwin_wayland.cpp     # ✅ implementiert (Phase 2B)
│   │   │           ├── mutter_headless.cpp  # ⏸ Stub
│   │   │           ├── wlroots_headless.cpp # ⏸ Stub
│   │   │           ├── xorg_nvidia.cpp      # ⏸ Stub
│   │   │           ├── xorg_dummy.cpp       # ⏸ Stub
│   │   │           ├── amdgpu_param.cpp     # ⏸ Stub
│   │   │           └── evdi.cpp             # ⏸ Stub
│   │   ├── windows/                   # Apollo-Erbe — wir bauen das nicht
│   │   └── macos/                     # Apollo-Erbe — wir bauen das nicht
│   ├── process.cpp / .h               # ⭐ Phase 2C: #elif __linux__ neben #ifdef _WIN32
│   ├── nvhttp.cpp                     # NvHTTP Pairing Server
│   ├── rtsp.cpp                       # RTSP Stream-Setup, dynamicRangeMode
│   ├── stream.cpp                     # ENet / RTP
│   ├── video.cpp / .h                 # Encoder-Pipeline (HDR-Stubs aktuell)
│   ├── nvenc/                         # NVENC-Wrapper (Windows-zentriert, Linux via CUDA-bridge)
│   ├── confighttp.cpp                 # WebUI-API-Handler
│   ├── display_device.cpp             # Apollo-Display-Layer (libdisplaydevice-Wrapper, Linux-stub)
│   ├── audio.cpp, input.cpp, ...
│   └── main.cpp
├── src_assets/
│   ├── common/assets/web/             # WebUI Quellen (Vue 3 + EJS-Preprocess via Vite)
│   │   └── (Apollos Bootstrap-UI — Phase 5 → PrimeVue 4)
│   ├── linux/misc/                    # 60-sunshine.rules (udev), 60-sunshine.conf (sysctl)
│   ├── linux/assets/                  # apps.json, OpenGL-Shaders
│   ├── windows/                       # Apollo-Erbe
│   └── macos/                         # Apollo-Erbe
├── packaging/
│   ├── linux/                         # AppImage / Arch / Fedora / Flatpak Manifeste
│   └── (sunshine.rb)                  # Homebrew (irrelevant für uns)
├── scripts/
│   └── linux_build.sh                 # Apollos Build-Script (wird durch Phase-3-Installer abgelöst)
├── tests/                             # gtest, BUILD_TESTS=OFF default
├── third-party/                       # 17 Submodules + glad/nvfbc/sudovda vendoriert
└── tools/                             # Apollo-Hilfstools (großteils Windows)
```

**Stern-Markierungen** (⭐) sind die Sonnenschein-spezifischen Files. Alles andere ist Apollo-geerbt und sollte beim Sunshine-Upstream-Sync (Phase 7+) weitgehend übernehmbar sein.

---

## 8. Submodules

17 direkte + nested. `git submodule update --init --recursive` ist Pflicht nach `git clone` und nach jedem `git pull` mit Submodul-Änderungen.

| Pfad | Repo | Branch | Pin |
|---|---|---|---|
| `packaging/linux/flatpak/deps/flatpak-builder-tools` | flatpak/flatpak-builder-tools | master | floating |
| `packaging/linux/flatpak/deps/shared-modules` | flathub/shared-modules | master | floating |
| `third-party/build-deps` | LizardByte/build-deps | dist | floating, **~1.1 GB** (Pre-built FFmpeg + Boost + x264 + x265 + SvtAv1Enc + cbs + hdr10plus für 9 Plattformen — wir nutzen nur `Linux-x86_64`) |
| `third-party/doxyconfig` | LizardByte/doxyconfig | master | floating |
| `third-party/googletest` | google/googletest | main | floating |
| `third-party/inputtino` | games-on-whales/inputtino | stable | floating |
| `third-party/libdisplaydevice` | LizardByte/libdisplaydevice | master | floating, **Linux noch als Stub markiert** |
| `third-party/nanors` | sleepybishop/nanors | master | floating |
| `third-party/nv-codec-headers` | FFmpeg/nv-codec-headers | sdk/12.0 | floating |
| `third-party/nvapi-open-source-sdk` | LizardByte/nvapi-open-source-sdk | sdk | floating |
| `third-party/Simple-Web-Server` | ClassicOldSong/Simple-Web-Server | master | floating, Apollo-Fork |
| `third-party/TPCircularBuffer` | michaeltyson/TPCircularBuffer | master | floating, macOS-only |
| `third-party/tray` | LizardByte/tray | master | **gepinnt auf `7936cb35`** (letzter Pre-Qt-Commit) — siehe Bekannte Probleme |
| `third-party/ViGEmClient` | LizardByte/Virtual-Gamepad-Emulation-Client | master | floating, Windows-only |
| `third-party/wayland-protocols` | wayland/wayland-protocols (gitlab.fdo) | main | floating |
| `third-party/wlr-protocols` | wlroots/wlr-protocols (gitlab.fdo) | master | floating |
| `third-party/moonlight-common-c` | ClassicOldSong/moonlight-common-c | master | floating, Apollo-Fork. Hat nested submodule `enet` |
| `third-party/moonlight-common-c/enet` | cgutman/enet | (nested) | floating |

**Vendoriert (nicht-Submodule, direkt im Repo)**:
- `third-party/glad/` — OpenGL-Loader
- `third-party/nvfbc/` — NVIDIA NvFBC Headers
- `third-party/sudovda/` — Apollos Windows-VDisplay-Treiber-Header (irrelevant für uns aber drin)

---

## 9. Bekannte Probleme & Workarounds

### 9.1 Ubuntu 24.04 libva 2.20 fehlt `vaMapBuffer2`

**Symptom**: Linker-Fehler `undefined reference to 'vaMapBuffer2'` aus `hwcontext_vaapi.c` (build-deps' FFmpeg).

**Ursache**: Ubuntu 24.04 Stock libva ist 2.20 (API 1.20). build-deps' FFmpeg wurde gegen libva ≥2.22 (API 1.21+) gebaut.

**Workaround (WSL)**: libva 2.22 from source bauen (Anleitung in `docs/building.md`):
```bash
DEBIAN_FRONTEND=noninteractive apt-get install -y meson python3-mako libxcb-dri3-dev libxcb-present-dev libxcb-sync-dev libxcb-xfixes0-dev libx11-xcb-dev
git clone --depth 1 -b 2.22.0 https://github.com/intel/libva.git /tmp/libva
cd /tmp/libva
meson setup build --prefix=/usr --libdir=lib/x86_64-linux-gnu --buildtype=release -Dwith_x11=yes -Dwith_wayland=yes -Dwith_glx=yes
ninja -C build install
```

**Auf realen Distros nicht nötig**: CachyOS hat libva 2.23, Fedora 41+ hat libva 2.22+, Arch hat libva 2.23. Nur WSL2 Ubuntu 24.04 betroffen.

### 9.2 Node 18 zu alt für Vite 6

**Symptom**: `crypto.hash is not a function` beim `npm run build`.

**Ursache**: Apollo's `package.json` will Node ≥20.19 (Vite 6 + @vitejs/plugin-vue 6).

**Workaround**: Node 20 via NodeSource installieren:
```bash
curl -fsSL https://deb.nodesource.com/setup_20.x | bash -
apt-get install -y nodejs
```

### 9.3 Tray-Submodule nach Qt-Migration nicht mehr GTK-kompatibel

**Symptom**: `tray_linux.cpp:16:10: fatal error: QAction: No such file or directory`.

**Ursache**: LizardByte/tray master ist mit Commit `d05166f` auf Qt umgezogen (`tray_linux.cpp`). Apollo's CMake erwartet aber `tray_linux.c` (GTK + AppIndicator).

**Workaround**: tray-Submodule auf den letzten Pre-Qt-Commit `7936cb35` gepinnt (siehe `.gitmodules` bzw. das Submodule-Pointer in `third-party/tray`). Phase-1.3-Commit `a95f2ee` hat das fixiert.

**Zukunft**: irgendwann müssen wir zur Qt-tray umsteigen oder `libnotify` direkt nutzen (was wir eh schon tun). System-Tray ist nicht kritisch für Sonnenschein.

### 9.4 CachyOS Boost 1.91 partial install

**Symptom**: `find_package(Boost ... COMPONENTS system REQUIRED)` schlägt fehl, obwohl `BoostConfig.cmake` da ist.

**Ursache**: CachyOS' boost-Paket installiert das umbrella `BoostConfig.cmake` aber nicht alle per-component `boost_<comp>-<ver>/`-Verzeichnisse. `boost_system-1.91.0/` fehlt.

**Workaround**: `cmake/dependencies/Boost_Sunshine.cmake` macht jetzt einen Pre-Flight-Check (`file(GLOB)` in `/usr/lib/cmake/boost_<comp>-*`) und fällt komplett auf FetchContent zurück, wenn auch nur eine Component fehlt. Verhindert die ALIAS-Target-Kollision, die mein erster Fix-Versuch hatte. Commit: `b6f263e`.

### 9.5 Arch/CachyOS CUDA-Pfad

**Symptom**: `Looking for a CUDA compiler - NOTFOUND` obwohl CUDA installiert ist.

**Ursache**: Arch packt nvcc nach `/opt/cuda/bin/nvcc`, nicht in PATH und nicht in CMakes Default-Suchpfad.

**Workaround**: `cmake/compile_definitions/linux.cmake` hat jetzt vor `check_language(CUDA)` einen `find_program(_sns_nvcc nvcc PATHS /opt/cuda/bin /usr/local/cuda/bin NO_DEFAULT_PATH)` und primt damit `CMAKE_CUDA_COMPILER`. Commit: `b6f263e`.

### 9.9 CUDA 13.2 inkompatibel mit GCC 16+

**Symptom**: `check_language(CUDA)` / `enable_language(CUDA)` crasht cmake mit 100+ Fehlern in `/usr/include/c++/16.1.1/type_traits`: `identifier "char8_t" is undefined`, `identifier "requires" is undefined`, etc.

**Ursache**: CUDA 13.2's `cudafe++` (der CUDA-Frontend-Compiler) kann die C++20-Features in GCC 16's libstdc++ Headers nicht parsen. CUDA 13.x unterstützt maximal GCC 15.

**Workaround**: `cmake/compile_definitions/linux.cmake` prüft jetzt `CMAKE_CXX_COMPILER_VERSION >= 16.0` vor `check_language(CUDA)`. Wenn GCC zu neu → CUDA wird automatisch übersprungen mit klarer Warnung. NVENC-Encoding (das eigentliche Feature) ist nicht betroffen — es nutzt die CUDA Runtime-API, nicht Device-Kompilierung. Commit: `5e1b1cf`.

**Zukunft**: Sobald NVIDIA CUDA 14+ mit GCC-16-Support released, die Versionsprüfung anpassen. Alternativ: `-DCMAKE_CUDA_HOST_COMPILER=/usr/bin/gcc-15` wenn user GCC 15 parallel installiert hat.

### 9.6 WSL2 `/tmp` wird beim Restart geleert

**Symptom**: Build-Verzeichnis ist nach WSL-Neustart weg, cmake muss komplett neu konfigurieren.

**Workaround**: Build-Dir auf `/root/snsbuild` (oder `/home/<user>/snsbuild`) legen, nicht `/tmp/snsbuild`.

### 9.7 kscreen-doctor SIGABRT in TTY/SSH

**Symptom**: `qt.qpa.xcb: could not connect to display` + SIGABRT, wenn `kscreen-doctor` aus SSH oder reinem TTY ausgeführt wird.

**Ursache**: kscreen-doctor ist Qt-basiert, will Wayland-Socket oder X11-Display.

**Workaround**: aus einer Konsole-App **innerhalb der laufenden Plasma-Wayland-Sitzung** ausführen (DBus-Session-Bus + WAYLAND_DISPLAY werden geerbt). Nicht aus SSH oder reinem TTY.

**Konsequenz für Sonnenschein**: Phase-3-Installer muss systemd `--user`-Modus als Default empfehlen (NICHT system-mode), damit Sonnenschein die DBus-Session erbt.

### 9.8 CMake `project(Apollo)` Branding nicht umbenannt

**Symptom**: Logs sagen "Apollo", Binary heißt `sunshine`, nicht `sonnenschein`. Verwirrend.

**Status**: Phase 1.6 (CMake-Rebrand) wurde bewusst aufgeschoben bis nach Phase 2. Berührt viele Pfade, Configs, Service-Files — Risiko, den Build zu brechen. Wird nach Phase-2-Done und vor Phase 3 gemacht.

### 9.10 Binary braucht `cap_sys_admin` für KMS-Capture

**Symptom**: `Failed to gain CAP_SYS_ADMIN`, `Couldn't get handle for DRM Framebuffer`, `Unable to initialize capture method`, alle Encoder scheitern → `Fatal: Unable to find display or encoder during startup.`

**Ursache**: KMS-Capture (DRM-Framebuffer-Zugriff) benötigt die Linux-Capability `CAP_SYS_ADMIN`. Ohne sie kann Sonnenschein den Bildschirminhalt nicht abgreifen → kein Bild zum Encoden → alle Encoder scheitern.

**Workaround (manuell)**:
```fish
sudo setcap cap_sys_admin+p (readlink -f ~/sonnenschein/build/sunshine)
```
Hinweis: `setcap` kann nicht auf Symlinks arbeiten, daher `readlink -f` um den echten Pfad aufzulösen.

**Zukunft**: Phase-3-Installer wird das automatisch via udev-Rules + systemd-Service-Config (`AmbientCapabilities=CAP_SYS_ADMIN`) setzen.

---

## 10. Letzte Commits chronologisch

(neueste zuerst, Format: `hash` — Beschreibung — Tag)

```
8126f26 — fix(cmake): don't FATAL_ERROR on CUDA when GCC skip was intentional — 2026-05-10
4fff349 — docs: add CLAUDE.md, SESSION_PROMPT.md, update STATUS.md — 2026-05-10
5e1b1cf — fix(cmake): skip CUDA when host GCC > 15 (cudafe++ incompatible) — 2026-05-10
b6f263e — fix(cmake): pre-flight Boost components on disk + auto-find Arch nvcc — 2026-05-10
895295c — fix(cmake): detect partial Boost installs and fall back to FetchContent — 2026-05-10
29298db — fix(cmake): accept any system Boost >= 1.89 instead of EXACT pin — 2026-05-10
72cf5c1 — Phase 2C: Wire virtual-display abstraction into process.cpp on Linux — 2026-05-10
509e87a — Phase 2B: KWin-Wayland virtual-display backend implementation — 2026-05-10
7cdd3f0 — Phase 2A: Virtual-display abstraction foundation (Linux) — 2026-05-09
539d3a5 — Phase 1.7: Rewrite docs/building.md for Sonnenschein — 2026-05-09
a95f2ee — Phase 1.3: Init submodules + pin tray pre-Qt — 2026-05-09
235920b — Initial import: Apollo codebase as Sonnenschein basis — 2026-05-09
```

`main` Branch zeigt nur auf `235920b` (initial import). `dev` ist 11 Commits voraus.

**Auf `dev` aktuell HEAD = `8126f26`** (Stand 2026-05-10).

---

## 11. Datei-Inventar (was wir geschrieben haben)

Liste der Dateien, die durch Sonnenschein neu sind oder substantiell geändert wurden. Apollos Original-Dateien sind nicht aufgelistet.

### Repo-Root + Doku
- `CLAUDE.md` (NEU) — Pflichtlektüre für Claude-Code
- `README.md` (REWRITE) — DE/EN Sonnenschein-Style
- `NOTICE` (REWRITE) — Sunshine + Apollo + docker-steam-headless Attribution
- `package.json` (PATCH) — name → "sonnenschein"
- `.gitignore` (EXTEND) — `_reference/`, `*.log`, etc.
- `docs/STATUS.md` (NEU) — DIESE DATEI
- `docs/ROADMAP.md` (NEU) — 8-Phasen-Vision
- `docs/SESSION_PROMPT.md` (NEU) — Session-Kickoff-Vorlage
- `docs/building.md` (REWRITE) — Linux-only Build-Anleitung mit Distro-Lists + Troubleshooting

### Community / GitHub
- `.github/CONTRIBUTING.md` (REWRITE)
- `.github/CODE_OF_CONDUCT.md` (NEU)
- `.github/SECURITY.md` (NEU)
- `.github/PULL_REQUEST_TEMPLATE.md` (NEU)
- `.github/ISSUE_TEMPLATE/{bug,feature,crash,config}.yml` (NEU)
- `.github/copilot-instructions.md` (REWRITE) — Linux-first
- `.github/workflows/lint.yml` (NEU)
- `.github/workflows/build-linux.yml` (NEU)

### CMake-Patches
- `cmake/dependencies/Boost_Sunshine.cmake` (PATCH) — Pre-flight + FetchContent-fallback
- `cmake/compile_definitions/linux.cmake` (PATCH) — virtual_display sources eingehängt, CUDA-Pfad-Auto-Detection (/opt/cuda), tray_linux.c (statt .cpp wegen Pin)

### C++ — virtual_display Modul (NEU, 14 Files)
- `src/platform/linux/virtual_display/types.h` — Enums + Structs
- `src/platform/linux/virtual_display/interface.h` — IBackend
- `src/platform/linux/virtual_display/detection.h+cpp` — Environment-Detection
- `src/platform/linux/virtual_display/factory.h+cpp` — Backend-Auswahl
- `src/platform/linux/virtual_display/subprocess.h+cpp` — popen/fork+exec Helper
- `src/platform/linux/virtual_display/backends/all.h` — make_*-Deklarationen
- `src/platform/linux/virtual_display/backends/kwin_wayland.cpp` — ✅ implementiert
- `src/platform/linux/virtual_display/backends/{mutter_headless,wlroots_headless,xorg_nvidia,xorg_dummy,amdgpu_param,evdi}.cpp` — Stubs
- `src/platform/linux/virtual_display/README.md` — Architektur

### C++ — process.cpp Wiring (Phase 2C)
- `src/process.h` (PATCH) — `#elif __linux__` Header-Include + zwei private Member
- `src/process.cpp` (PATCH) — Linux-Branch in `execute()` + `terminate()`

### Submodule-Pin
- `third-party/tray/` — gepinnt auf `7936cb35` (vor `.gitmodules`-Datei; gitlink im Tree)

---

## 12. Was als nächstes — konkrete Schritte

In Reihenfolge der Priorität.

### A) Virtual-Display-Trigger testen (Phase 2D, nächster Schritt)

1. **Auf CachyOS**: `headless_mode = true` in `~/.config/sunshine/sunshine.conf` setzen (Datei editieren oder anlegen falls nicht vorhanden). Alternativ per-App in `apps.json` mit `"virtual_display": true`.
2. Sonnenschein neu starten: `./build/sunshine-0.0.0 2>&1 | tee ~/sonnenschein-test.log`
3. SteamDeck via Moonlight verbinden → "Desktop" starten.
4. **Beobachten**: Logs müssen `Sonnenschein: virtual display 'Sonnenschein-XXXXXXXX' created via backend 'kwin_wayland'` zeigen.
5. `kscreen-doctor outputs` → neuer Output muss in der Liste sein.
6. Disconnect → Output muss verschwinden.

### B) Phase 1.6 — CMake-Rebrand (nach 2D)

Sobald Phase 2D grün ist und ein erster Virtual Display erfolgreich erzeugt wurde:
- `CMakeLists.txt`: `project(Sonnenschein)`
- `cmake/prep/build_version.cmake`: `Sunshine Branch:` → `Sonnenschein Branch:`
- Add-Executable-Target: `sunshine` → `sonnenschein` (Symlink `sunshine` für Rückwärts-Kompat)
- Service-Files / .desktop-Files: `dev.lizardbyte.app.Sunshine` → eigener FQDN
- README/Docs: alle "Sunshine"-Erwähnungen prüfen

### C) Phase 3 — Installer

Sobald 1.6 done. Erste Iteration: nur Arch (CachyOS-Test-Hardware). Dann iterativ andere Distros.

### D) Phase 4 — HDR & AV1

Nach Phase 3, weil Installer braucht systemd-User-Mode-Setup für DBus-Bus zur HDR-Kommunikation mit KWin.

### E) Sunshine-Upstream-Sync

Spätestens Phase 7. Ein Script `scripts/sync-from-sunshine.sh` automatisieren, das Sunshine cherry-pickt für sicherheitsrelevante Fixes.

---

## 13. Update-Konventionen für diese Datei

**Regel 1**: Diese Datei wird nach jedem abgeschlossenen Schritt aktualisiert. Nicht erst am Ende einer Session — sofort.

**Regel 2**: Aktualisierungen passieren in folgenden Abschnitten:
- **TL;DR** — kurzer Satz zum aktuellen Zustand, immer top-of-mind.
- **Phasen-Status (§6)** — Aufgaben-Tabelle mit ✅/🟡/🔴/⏸ aktualisieren, Commit-Hashes nachtragen.
- **Bekannte Probleme (§9)** — neue Bugs + Workarounds dokumentieren, mit Reproduktions-Befehl + Fix-Commit.
- **Letzte Commits (§10)** — neue Commits oben anhängen.
- **Datei-Inventar (§11)** — neu angelegte oder substantiell geänderte Files dazuschreiben.
- **Was als nächstes (§12)** — Reihenfolge anpassen, abgeschlossene Punkte streichen, neue dazuschreiben.

**Regel 3**: Stand-Datum oben (Zeile 6) bei jeder Aktualisierung neu setzen.

**Regel 4**: Wenn der Maintainer eine neue Entscheidung fällt (z.B. "wir nutzen jetzt doch Bootstrap statt PrimeVue"): unter §4 (Tech-Stack-Entscheidungen) als zusätzliche Zeile festhalten, mit Datum + Begründung.

**Regel 5**: Wenn ein Commit, der hier erwähnt ist, gerebased oder amended wird: hier den Hash aktualisieren. Dieser Markdown ist die Autorität für "welcher Commit hat was getan", nicht ein vergessenes Sticky-Note.

**Regel 6**: Wenn unklar ob etwas in STATUS.md gehört: lieber rein als raus. Diese Datei ist der einzige Schutz gegen Multi-Session-Kontext-Verlust.
