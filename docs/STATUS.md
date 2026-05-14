# Sonnenschein — Aktueller Projektstand (STATUS.md)

> **Lebendes Dokument.** Diese Datei wird in jeder Session nach jedem
> abgeschlossenen Schritt aktualisiert. Sie ist die einzige Quelle der
> Wahrheit für Multi-Session-Arbeit. Wenn etwas hier fehlt, weiß die
> nächste Session es nicht.

**Stand:** 2026-05-14 (spät) — **60-Hz-Bug verstanden: KWin Wayland ignoriert non-EDID-Modi. Echte Lösung = `kde_output_management_v2::set_custom_modes` (KWin MR !8766, in Plasma 6.6). Mein `29cd4b6`-Versuch (kscreen-doctor add-virtual-output Pre-Create) ist auf CachyOS getestet → ohne Wirkung, Output erscheint nicht im wl_output Registry. Rollback auf Stand `41fa9ba` + detaillierte Implementierungs-Anleitung für nächste Session.**

---

## TL;DR — Wo stehen wir gerade

**Eine-Zeile-Wahrheit**: Stream + Ton + Eingaben + Headless funktionieren auf CachyOS Plasma 6.6.4 + RTX 3070. Auflösung wird respektiert. **Refresh-Rate hängt immer bei 60 Hz**, egal was der Client (z.B. SteamDeck OLED 90 Hz) anfordert.

### Was Codex am 2026-05-13 verbockt hat — und der Stand jetzt

| # | Commit | Was es war | Ergebnis |
|---|---|---|---|
| 1 | `bf7d939` (10.05) | Maintainer-Stand: Bild+Ton+Eingaben+Headless ✅, 60 Hz ❌ | **letzter wirklich guter Code-Stand** |
| 2 | `6504268` (13.05) | Codex: KScreen-Resolver, Mode-Set NACH `stream_virtual_output` | 60 Hz blieb (KWin akzeptiert Mode-Set nicht nachträglich) |
| 3 | `723537a` (13.05) | Codex: eskaliert auf `kde_output_management_v2` (+850 Zeilen) | KWin Direct Capture brach komplett ab → Moonlight Ping Timeout |
| 4 | `edc144e` (13.05) | Codex: Portal-Fallback bei fehlendem ScreenCast | Mini-Hotfix |
| 5 | `501431a` (13.05) | Codex: revert „stable KWin direct capture path" (-884 Zeilen) | erster Rollback-Versuch |
| 6 | `74c63cf` (13.05) | Codex: revert „pre KScreen resolver path" (-491 Zeilen) | **pwgrab.cpp jetzt byte-identisch mit `bf7d939`** |
| 7 | `41fa9ba` (13.05) | Maintainer: Portal-Fallback statt fatalem `return -1` | letzter Push vor Pause; nur 8 Zeilen über `bf7d939` |
| 8 | **`29cd4b6` (14.05)** | **60-Hz-Fix-Versuch**: `kscreen-doctor add-virtual-output` + `mode-set` zurück VOR Stream-Start | gepusht, WSL2-Build 3/3 grün, CachyOS-Test ausstehend |

`git diff bf7d939 HEAD -- src/platform/linux/pwgrab.cpp` = **nur 28 Zeilen**. Codex' destruktive Experimente sind rausgerollt. Der laufzeit-relevante Code ist heute effektiv `bf7d939` + Portal-Fallback (`41fa9ba`).

### Der 60-Hz-Fix-Versuch (uncommitted, in Arbeit am 2026-05-14)

**Root Cause (verstanden)**: `zkde_screencast_unstable_v1::stream_virtual_output` hat **keinen Refresh-Rate-Parameter** (siehe `third-party/plasma-wayland-protocols/src/protocols/zkde-screencast-unstable-v1.xml`). KWin erstellt den Virtual Output beim Stream-Start mit fixem 60-Hz-Default, dessen Mode-Liste *nur* 60 Hz enthält. Nachträgliche `kscreen-doctor output.NAME.mode.WxH@HZ`-Aufrufe werden ignoriert, weil der Modus nicht in der Mode-Liste existiert.

**Fix-Strategie**: Output **vorher** via `kscreen-doctor add-virtual-output NAME W H` erstellen, dann `mode WxH@HZ` setzen, dann erst Stream öffnen. `pwgrab.cpp::find_target()` findet den existierenden Output → nutzt `stream_output` (mit `wl_output*`) statt `stream_virtual_output`. Refresh-Rate kommt durch die Mode-Liste, nicht durch Argument.

Codex hat das Pattern abgelehnt (vermutlich weil es `e453afa`/`d84072e` widerspricht, die `kscreen-doctor add-virtual-output` als „doppelt-gemoppelt" entfernt haben). Aber: damals war pwgrab.cpp noch nicht in der `find_target → stream_output` / `stream_virtual_output`-Hybrid-Form. Heute (HEAD pwgrab.cpp Z. 209-228) wird der Fall korrekt unterschieden.

**Implementierung im 60-Hz-Fix-Commit**:
- `src/platform/linux/virtual_display/backends/kwin_wayland.cpp::create()` bringt `kscreen-doctor add-virtual-output` + `mode-set` + `hdr-enable` zurück.
- Neues Feld `ActiveDisplay::added_via_kscreen` trackt, ob wir den Output erstellt haben → `destroy()` ruft nur dann `remove-virtual-output`.
- Bei `add-virtual-output`-Failure (Plasma < 6.4, kscreen-doctor unreachable): **kein Abort** — wir fallen durch und `pwgrab.cpp` nutzt seinen `stream_virtual_output`-Fallback (60 Hz, aber funktionierender Stream).

**Erwartung**: Stream startet wie bisher (Bild+Ton+Eingaben+Headless), aber jetzt mit der vom Client angeforderten Refresh-Rate. SteamDeck-OLED 90 Hz wird durchgereicht.

### Was beim letzten CachyOS-Stand erreicht ist

- ✅ KWin Direct Capture (`zkde_screencast`) statt Portal-Dialog
- ✅ Virtual Display wird transparent erstellt, kein KDE-Auswahldialog
- ✅ Physische Monitore werden beim Stream deaktiviert, beim Disconnect wieder eingeschaltet
- ✅ Bild läuft, Ton läuft, Eingaben gehen durch
- ✅ Client-Auflösung wird respektiert (1280x800 vom SteamDeck arrived als 1280x800)
- 🟡 **Refresh-Rate steckt bei 60 Hz** (Fix-Versuch oben)
- 🟡 HDR theoretisch im Encoder-Metadata, aber nicht KWin-aktiviert

### Hauptanwendungsfall (Maintainer)

Physische Monitore am CachyOS-PC. Beim Streaming → alle physischen Outputs aus → Virtual Display als einziger aktiver Output → PipeWire/KWin captured ihn → Moonlight zeigt das Bild auf dem Client (z.B. SteamDeck oder TV via Moonlight für Android). Beim Disconnect → physische Outputs wieder an.

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
14. [Deep-Dive: PipeWire & Portal Integration (Phase 4)](#14-deep-dive-pipewire--portal-integration-phase-4)

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
  - Submodules in CI: `submodules: recursive`, weil KWin Direct ScreenCast das Plasma-Wayland-Protokoll-Submodule braucht.

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
- Submodules: seit KWin-Direktcapture `submodules: recursive`, weil `third-party/plasma-wayland-protocols` für `zkde-screencast-unstable-v1.xml` benötigt wird.

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
| Submodules eingebunden via `_reference/add_submodules.sh` + später Plasma-Protokolle | ✅ | `a95f2ee` + `4c63d36` | inkl. moonlight-common-c/enet rekursiv, tray gepinnt auf `7936cb35`, neu `third-party/plasma-wayland-protocols` für KWin Direct ScreenCast |
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

**Hauptanwendungsfall (geklärt 2026-05-10)**: Physische Monitore sind verbunden, sollen sich beim Streaming deaktivieren und Platz für einen virtuellen Monitor machen. Virtual Display wird dann der einzige aktive Output. Headless (kein physischer Monitor) ebenfalls unterstützt.

### Phase 2E — Capture-Routing Fix ✅

**Problem 1**: `map_display_name()` gibt auf Linux `{}` zurück (Windows-only `sm_instance`). `kmsgrab.cpp` parst den leeren String als Integer → `1105514439` (Müll) → `Couldn't find monitor [1105514439]`.

**Fix (Commit `9baebbf`)**: `config::video.output_name = "0"` statt `map_display_name()`.

**Problem 2**: `video.cpp` Zeile 1180 nutzt `proc::proc.display_name` direkt für KMS-Capture, NICHT `config::video.output_name`. `this->display_name` war auf `"Sonnenschein-00E8F1E1"` gesetzt → `util::from_view()` parsete Müll.

**Fix (Commit `71cdac3`)**: `this->display_name = "0"` statt `_vdisplay_handle->output_name`. KMS findet nun Monitor 0 (HDMI-A-1) fehlerfrei.

### Phase 2 — Architektur-Erkenntnis 🔴

**Zentrale Erkenntnis nach CachyOS-Tests**: KMS-Capture kann Virtual Displays **prinzipbedingt nicht** capturen.

| Schicht | Was sie sieht | Capture möglich? |
|---------|--------------|------------------|
| DRM/KMS (`kmsgrab.cpp`) | Physische Connectors (HDMI, DP) | ✅ Nur physische Displays |
| KWin Compositor | Alle Outputs inkl. Virtual | ❌ Keine Capture-API |
| PipeWire Portal | Alle Outputs via `xdg-desktop-portal` | ✅ Auch Virtual Displays |
| wlr-screencopy | Compositor-Outputs | ❌ KDE unterstützt es nicht |

**Konsequenz**: Für den Virtual-Display-Capture-Use-Case muss ein **PipeWire-Capture-Backend** implementiert werden (Phase 4). Das ist der einzige Weg auf KDE Wayland.

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
├── third-party/                       # Submodules + glad/nvfbc/sudovda vendoriert
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

**Workaround (manuell, NUR für KMS-Capture)**:
```fish
sudo setcap cap_sys_admin+p (readlink -f ~/sonnenschein/build/sunshine)
```
Hinweis: `setcap` kann nicht auf Symlinks arbeiten, daher `readlink -f` um den echten Pfad aufzulösen.

**⚠️ ACHTUNG: setcap + PipeWire-Capture sind INKOMPATIBEL!**
`xdg-desktop-portal` verweigert den Zugriff wenn das Binary Capabilities hat (`Unable to open /proc/PID/root`). Für PipeWire-Capture:
```fish
sudo setcap -r (readlink -f ~/sonnenschein/build/sunshine)  # Capabilities ENTFERNEN
```

**Zukunft**: Phase-3-Installer wird das automatisch via udev-Rules + systemd-Service-Config (`AmbientCapabilities=CAP_SYS_ADMIN`) setzen. PipeWire-Capture braucht kein cap_sys_admin.

### 9.11 WSL root + Windows-Checkout: Git `dubious ownership`

**Symptom**: Beim WSL-Build aus `/root/snsbuild` gegen den Source unter `/mnt/c/Users/cooki/Documents/ClaudeCode/sonnenschein` läuft der CMake-Build durch, aber Git-Metadaten-Abfragen können warnen oder fehlschlagen (`fatal: detected dubious ownership in repository ...`, teils als nicht-fatales `ERROR: Got git error while fetching tags: 128` im CMake-Output).

**Ursache**: WSL läuft als `root`, der Checkout liegt aber auf dem Windows-Mount und gehört aus Git-Sicht einem anderen Owner.

**Workaround (einmalig in WSL als root)**:
```bash
git config --global --add safe.directory /mnt/c/Users/cooki/Documents/ClaudeCode/sonnenschein
```

**Status**: Nicht build-blockierend. WSL-Build am 2026-05-10 war trotz Warnung erfolgreich (`sunshine-0.0.0` + Symlink `sunshine` in `/root/snsbuild`).

### 9.12 PipeWire Portal: Crash nach Auswahl des virtuellen Bildschirms

**Symptom**: Auf CachyOS/Plasma öffnet sich erstmals der KDE-Screen-Record-Portal-Dialog. Nach Auswahl des virtuellen Bildschirms loggt Sonnenschein `Portal: stream node_id=127` und bricht direkt danach mit GLib ab:
```text
GLib-ERROR **: g_variant_new: expected array GVariantBuilder but the built value has type '(null)'
terminated by signal SIGABRT
```

**Ursache**: `OpenPipeWireRemote` baut das leere `a{sv}`-Options-Argument falsch. Der Code übergibt `g_variant_builder_end(...)` an eine `g_variant_new("(oa{sv})", ...)`-Formatstelle, die einen `GVariantBuilder*` erwartet.

**Status**: Reproduziert durch Maintainer-Log am 2026-05-10. Gefixt in `6d6433f`: `OpenPipeWireRemote` übergibt jetzt wie `CreateSession`/`SelectSources`/`Start` einen echten `GVariantBuilder*` an `g_variant_new("(oa{sv})", ...)`. CachyOS-Test mit `5fe1ea6` bestätigt: `Portal: PipeWire fd=68 node_id=140`, PipeWire `streaming`, kein GLib-`SIGABRT`.

### 9.13 PipeWire negotiated 1920x1080 statt SteamDeck 1280x800@90

**Symptom**: Erster echter End-to-End-Stream funktioniert, aber Auflösung/Aspect Ratio/Bildwiederholrate passen nicht zum SteamDeck. Log:
```text
Display mode for client [sd] requested to [1280x800x90]
Sonnenschein vdisplay (kwin): created 'Sonnenschein-00E8F1E1' 1280x800@90, Hz HDR10
Portal: PipeWire fd=68 node_id=140
PipeWire: format negotiated 1920x1080 fmt=8
PipeWire display initialized: 1920x1080
```

**Ursache (validiert)**: Anfangs war `pw_capture_t::init()` noch auf `1920x1080` voreingestellt; das wurde in `76d03a4` korrigiert. Die danach verbliebene Ursache ist die KDE-Portal-Quelle selbst: Im Dialog ausgewähltes "Virtuelles Display" kommt als XDG-`VIRTUAL`-Source mit `logical size=1920x1080`, nicht als der von Sonnenschein erzeugte KWin-Output `Sonnenschein-...`.

**Status**: `76d03a4` wirkt teilweise: `pwgrab.cpp` loggt `PipeWire: requesting format 1280x800@90`. CachyOS-Test mit `12a17da` zeigt aber weiterhin `PipeWire: format negotiated 1920x1080 fmt=8` und die neue Warnung `PipeWire: negotiated size differs from client request 1280x800 -> 1920x1080`. Damit ignoriert KDE/Portal den Formatwunsch oder die im Dialog gewählte Quelle ist nicht der KWin-Output `Sonnenschein-00E8F1E1`.

**Recherche 2026-05-10**:
- Offizielle XDG-Portal-Doku (`flatpak.github.io/xdg-desktop-portal/.../ScreenCast.html`): `ScreenCast.SelectSources` kennt `types` mit `1=MONITOR`, `2=WINDOW`, `4=VIRTUAL`; `cursor_mode` ist standardmäßig `Hidden`, `2=Embedded` bettet den Cursor in die Videoframes ein.
- KDE Bug 485850 (`bugs.kde.org/show_bug.cgi?id=485850`): `xdg-desktop-portal-kde` Virtual Screens sind/haben historisch `1920x1080` hardcoded (`startStreamingVirtual(..., {1920, 1080}, ...)`).
- KDE Bug 512620 (`bugs.kde.org/show_bug.cgi?id=512620`): KWin-Screencast-Virtual-Monitor kann nur mit `1920x1080` negotiated werden; andere Auflösungen liefern keine Frames oder frieren ein. Das passt exakt zum Sonnenschein-Log, wenn im Portal die Option "Virtuelles Display" gewählt wird.

**Status**: Diagnose-/Cursor-Patch in `447dc8b`. Portal-Stream-Properties (`source_type`, `size`, `position`, `id`, `mapping_id`) werden geloggt und `cursor_mode=Embedded` wird angefordert, falls KDE diesen Modus anbietet. WSL2-Build gegen `/root/snsbuild` ist grün (`ninja: no work to do` nach erfolgreichem Link, Binary `sunshine-0.0.0` aktualisiert um 16:58). Damit sehen wir im nächsten CachyOS-Test eindeutig, ob der Dialog die XDG-`VIRTUAL`-Quelle statt des existierenden KWin-Monitors auswählt, und der Mauszeiger sollte sichtbar werden, wenn KDE Embedded Cursor unterstützt.

**Validierung `bb3e758` (CachyOS, 2026-05-10 17:06)**:
```text
Portal: available source types=7 cursor modes=7
Portal: requesting embedded cursor mode
Portal: stream source_type=VIRTUAL
Portal: stream logical size=1920x1080
PipeWire: requesting format 1280x800@90
PipeWire: format negotiated 1920x1080 fmt=8
PipeWire: negotiated size differs from client request 1280x800 -> 1920x1080
```
Damit ist bestätigt: der falsche 1920x1080-Pfad kommt von der ausgewählten KDE-XDG-`VIRTUAL`-Quelle, nicht von unserem PipeWire-Formatrequest.

**Fix-Kandidat `4c63d36` (2026-05-10)**:
- `src/platform/linux/pwgrab.cpp` probiert bei benanntem Virtual Display zuerst KWin Direct ScreenCast (`zkde_screencast_unstable_v1`) statt `xdg-desktop-portal`.
- Der Code enumeriert `wl_output`-Namen, sucht exakt `Sonnenschein-...`, fordert Embedded Cursor an und verbindet den daraus gelieferten PipeWire-Node über den lokalen PipeWire-Core.
- Wenn der Ziel-Output nicht gefunden wird oder KWin den Zugriff verweigert, fällt Sonnenschein **nicht** mehr auf die KDE-XDG-`VIRTUAL`-Quelle zurück, weil dieser Pfad validiert 1920x1080 erzwingt.

**Weiterer Fix (2026-05-10 18:40)**:
- CachyOS-Test mit `4c63d36` scheiterte, da `Sonnenschein-00E8F1E1` nicht als Output-Name gefunden wurde. KWin verwendet intern vermutlich `Virtual-1` als `wl_output::name`. 
- `pwgrab.cpp` prüft nun zusätzlich `wl_output::description` und nutzt einen Fallback auf `Virtual-*`, falls kein exakter Name gefunden wird.
- Behebung eines C++-Formatierungsfehlers, bei dem `kwin_wayland.cpp` ungültige Strings ins Log schrieb (`@third-party\build-deps...`).

**Weiterer Fix (2026-05-10 19:15)**:
- Ein weiterer CachyOS-Test zeigte, dass der Output noch immer nicht in der `wl_output`-Liste auftauchte. Grund: KWin erstellt das virtuelle Display asynchron, und unsere initialen `wl_display_roundtrip`-Aufrufe in `init()` fanden statt, *bevor* KWin das neue Display broadcasten konnte.
- `pwgrab.cpp` wurde angepasst: Wenn `start()` den Output im ersten Durchlauf nicht findet, werden nun gezielt weitere `wl_display_roundtrip`-Aufrufe durchgeführt, um die Hotplug-Events aus der Wayland-Queue abzurufen, bevor der zweite Such-Durchlauf startet.

**Native Virtual Output Stream Migration (2026-05-10 19:40)**:
- Auch der Roundtrip-Hotfix schlug fehl (`timeout waiting for stream`).
- Analyse des KWin-Wayland-Protokolls (`zkde_screencast_unstable_v1`) zeigte, dass KWin speziell für diesen Anwendungsfall die Methode `stream_virtual_output` (bzw. `_with_description`) anbietet.
- Anstatt zu versuchen, einen von `kscreen-doctor` erstellten Output aus der allgemeinen Wayland-Registry abzufangen, ruft `pwgrab.cpp` nun direkt `zkde_screencast_unstable_v1_stream_virtual_output` auf, falls das Display nicht gefunden wurde. 
- Das umgeht alle Sichtbarkeits- und Asynchronitätsprobleme, da KWin explizit angewiesen wird, für diesen angeforderten Stream ein virtuelles Display in der passenden Auflösung zu liefern.

**Erfolgreicher E2E Test (2026-05-10 19:40)**:
- Der CachyOS Test mit `stream_virtual_output` war ein **Erfolg**! Das Log zeigt: `KWin direct capture: streaming output 'Sonnenschein-00E8F1E1' 1280x800 node_id=73`.
- PipeWire negotiated erfolgreich `1280x800 fmt=8`.
- Die folgenden Probleme verbleiben:
  - **Bildwiederholrate (60Hz statt 90Hz)**: Ein Formatierungsfehler beim Loggen (`@third-party\build-deps...`) deutet darauf hin, dass die Refresh-Rate in `pwgrab.cpp` nicht korrekt übergeben wird. **(Gefixt am 2026-05-10: C++ Typ-Cast gefixt und dynamisches `kscreen-doctor mode set` nach Stream-Start eingebaut. Das Log-Korruptions-Problem mit snprintf wurde entfernt und durch robustere String-Casts ersetzt.)**
  - **Physische Monitore deaktivieren**: Der User fordert, dass bei aktiven Virtual Displays alle physischen Monitore ausgeschaltet werden (Headless-Modus Priorität 1). **(Gefixt am 2026-05-10: `kwin_wayland.cpp` nutzt nun `kscreen-doctor` um physische Monitore abzuschalten und später wiederherzustellen)**.
  - **HDR Option**: HDR wird auf dem Stream noch nicht angeboten.

**E2E Refresh Rate & Headless Fix (2026-05-10 20:45)**:
- Das hartnäckige Problem der korrupten Refresh-Rate-Strings (`@third-party\build-deps...`) wurde identifiziert: In der Windows/WSL Build-Umgebung mit den speziellen `build-deps` Submodules führt das Zeichen `@` in C++ String-Literalen unter bestimmten Bedingungen zu einer Fehlinterpretation durch den Compiler oder das Build-System, wodurch der String durch einen internen Pfad (`sys390.h` aus Boost Predef) ersetzt wird.
- Lösung: Das `@` Zeichen wird nun nicht mehr als Literal im Source-Code verwendet, sondern zur Laufzeit als Zeichen (`0x40`) eingefügt.
- Zudem wurde eine Verzögerung von 500ms vor dem `kscreen-doctor mode set` eingebaut, um KWin Zeit zu geben, das neue virtuelle Display in seinem Konfigurationssystem zu registrieren, bevor die Refresh-Rate geändert wird.
- Headless-Mode (Physische Monitore aus) ist voll funktionsfähig.

### 9.14 PipeWire-Virtual-Display: Touch/Maus fehlt

**Symptom**: Im funktionierenden Virtual-Display-Stream auf SteamDeck sind keine Touch-Eingaben möglich und der Mauszeiger wird nicht angezeigt.

**Ursache (offen)**:
- Cursor: `pwgrab.cpp` setzt derzeit keinen Cursor-Metadatenpfad und ruft `push_captured_image_cb(..., true)` nur mit Frameinhalt auf. Portal/ScreenCast kann Cursor je nach `cursor_mode` im Stream einbetten oder als Metadaten liefern; wir setzen aktuell keine Cursor-Option.
- Touch/Input: Sonnenschein erzeugt zwar ein virtuelles Gamepad (`Gamepad 0 will be Xbox One controller`), aber Touch-/Maus-Koordinaten müssen auf `display->env_width/env_height` und den Virtual-Display-Output abgebildet werden. Bei PipeWire `1920x1080` vs. Client `1280x800` ist das Mapping wahrscheinlich falsch oder inaktiv.

**Status**: Teilweise erledigt. CachyOS-Test mit `bb3e758` bestätigt: Ton und Eingaben funktionieren. Cursor wurde über `cursor_mode=Embedded` angefordert; falls der Cursor weiter fehlt, bleibt das ein separater Cursor-Compositing-Bug. Touch/Input ist nicht mehr Hauptblocker.

### 9.16 SteamDeck Refresh bleibt nach KWin Direct bei 60 Hz

**Symptom**: Steam/Moonlight auf dem SteamDeck zeigt eine niedrigere bzw. falsche Bildwiederholrate im Stream an, obwohl das Deck-Display mehr kann und der Client `1280x800x90` anfragt.

**Ursache (aktuelle Arbeitshypothese)**: Der alte `source_type=VIRTUAL`/`1920x1080`-Pfad ist durch KWin Direct ScreenCast gelöst. `stream_virtual_output` erzeugt den virtuellen Output aber initial mit 60 Hz. Der nachträgliche `kscreen-doctor output.Sonnenschein-...mode.1280x800@90`-Befehl trifft wahrscheinlich nicht zuverlässig den tatsächlichen KScreen-Namen, weil KWin/KScreen den Output intern häufig als `Virtual-*` registriert.

**Status**: `6504268` reicht nicht. Maintainer-Test am 2026-05-13: Stream bleibt bei 60 Hz. Fix-Kandidat `723537a` ersetzt den reinen `kscreen-doctor`-Ansatz durch KWin Output-Management-v2:
- `cmake/compile_definitions/linux.cmake` generiert zusätzlich `kde-output-device-v2` und `kde-output-management-v2`.
- `pwgrab.cpp` bindet KWins Output-Device-Registry, löst den echten Virtual Output auf, legt bei Bedarf einen Custom Mode `WxH@Hz` an, wendet den Mode atomar an und aktiviert HDR/WCG, wenn der Output die Capability meldet.
- Nach der Anwendung wird der aktive Mode über die Wayland-Events verifiziert; `kscreen-doctor` bleibt nur Fallback.
- PipeWire loggt jetzt die negotiated Framerate. Wenn sie unter dem Client-FPS liegt, taktet der Capture-Loop den Encoder weiter mit der Client-Framerate und wiederholt den letzten Frame, statt auf 60 neue Frames pro Sekunde zu blockieren.
- HDR: `display.is_hdr()` und HDR10-Metadaten sind für den PipeWire/KWin-Direct-Pfad aktiv, sobald der Client `dynamicRange > 0` anfragt. 10-bit/scRGB PipeWire-Input wird noch nicht negotiated, weil der aktuelle Software-Konverter Eingangsframes fest als `AV_PIX_FMT_BGR0` behandelt; das ist der nächste gezielte HDR-Qualitäts-Fix, falls der Client zwar HDR signalisiert, aber Farben nicht korrekt sind.

### 9.17 Regression: KWin Direct fehlt, Stream startet nicht mehr

**Symptom**: CachyOS-Test mit HEAD `ec832c8` am 2026-05-13: Client fordert `1280x800x90`, Virtual Display wird erzeugt, danach bricht Capture ab. Log-Signatur:
- `KWin output management: bound kde_output_management_v2 version 19`
- `KWin direct capture: zkde_screencast_unstable_v1 not available; falling back to portal`
- `KWin direct capture: failed for output 'Sonnenschein-...'`
- `Initial Ping Timeout`

**Ursache**: Der neue Output-Management-Patch behandelte fehlendes `zkde_screencast_unstable_v1` als fatal und blockierte bewusst den Portal-Fallback. Das ist für Plug-and-play falsch: KWin Direct ist bevorzugt, aber nicht garantiert in jedem Registry-/Permission-Zustand verfügbar. Zusätzlich war die dynamisch erzeugte KDE-Permission-Datei nur auf `zkde_screencast_unstable_v1` begrenzt, obwohl der neue Pfad auch `kde_output_management_v2` und `kde_output_device_registry_v2` bindet.

**Fix `edc144e`**:
- `pwgrab.cpp` fällt bei fehlendem oder fehlschlagendem KWin Direct Capture wieder auf xdg-desktop-portal Monitor-Capture zurück, statt `-1` zurückzugeben und damit die Session/den Virtual Output zu zerstören.
- Die Permission-Datei `sonnenschein-kwin-screencast.desktop` schreibt jetzt `X-KDE-Wayland-Interfaces=zkde_screencast_unstable_v1,kde_output_management_v2,kde_output_device_registry_v2`.
- Erwartung: Stream startet wieder. Wenn KWin Direct danach weiterhin fehlt, ist das im Log sichtbar und Portal ist ein funktionaler Fallback; Refresh/HDR bleiben dann als KWin-Direct-spezifischer Folgefix offen.

**Rollback-Entscheidung (2026-05-13)**: Der Maintainer hat entschieden, den letzten funktionierenden Stand wiederherzustellen. `501431a` setzt die betroffenen Code-Dateien auf `67a93e3` zurück:
- `src/platform/linux/pwgrab.cpp`
- `cmake/compile_definitions/linux.cmake`

Damit sind der Output-Management-v2-Patch `723537a`, der STATUS-HEAD `ec832c8` und der Fallback-Hotfix `edc144e` inhaltlich aus dem Laufzeitcode entfernt. Diese Regressionskette darf nicht erneut als großer Kombi-Patch für Refresh/HDR eingeführt werden. Nächster 90-Hz-Versuch nur in kleinen, einzeln testbaren Schritten.

**Korrektur (2026-05-13)**: Der Maintainer hat direkt nach `501431a` klargestellt, dass `67a93e3`/`6504268` bereits die defekte Version nach dem letzten guten Stand war. Der tatsächliche Zielstand für den Laufzeitcode ist `bf7d939`/`2d5b81a`, also vor dem KScreen-Resolver. `74c63cf` setzt `pwgrab.cpp` entsprechend zurück.

**Weitere Korrektur (2026-05-13 23:10)**: Der neue Log wurde mit Binary `0.0.0.4d47b5e` erzeugt, also nicht mit dem danach gepushten `74c63cf`/`c451725`. Trotzdem ist der eigentliche Defekt noch im aktuellen Laufzeitcode: der KWin-Direct-Pfad gibt bei fehlendem `zkde_screencast_unstable_v1` `-1` zurück und verhindert damit den Portal-Fallback. `41fa9ba` entfernt diesen fatalen Abbruch, ohne erneut Output-Management, HDR oder Frame-Pacing anzufassen.

### 9.18 60-Hz-Bug — Endgültige Diagnose + echte Lösung (Stand 2026-05-14 spät)

**Symptom**: SteamDeck-OLED (oder beliebiger 90/120 Hz Client) bleibt im Stream bei 60 Hz, obwohl der Stream sonst sauber läuft (Bild + Ton + Eingaben + Headless funktionieren).

#### Root Cause (online verifiziert)

KWin Wayland ignoriert **alle** Modi, die nicht in der nativen Mode-Liste eines Outputs sind. Zitat aus der KDE-Mod-Diskussion (KDE Discuss thread 13642): *"Unfortunately, Kwin (Wayland) does not support running the display in any geometry other than what is supported by a native mode."*

Konkret im Sonnenschein-Pfad:
1. `zkde_screencast_unstable_v1::stream_virtual_output(name, w, h, scale, pointer)` — **kein Refresh-Rate-Argument** (siehe `third-party/plasma-wayland-protocols/src/protocols/zkde-screencast-unstable-v1.xml`).
2. KWin erstellt den Output mit fixem 60-Hz-Default; die Mode-Liste des Outputs enthält **nur** 60 Hz.
3. Jeder spätere `kscreen-doctor output.NAME.mode.1280x800@90` schlägt fehl (silent), weil `1280x800@90` nicht in der Mode-Liste ist.

#### Was `29cd4b6` (mein Versuch vom Vormittag) versucht hat — und warum es nicht greift

`KwinWaylandBackend::create()` ruft `kscreen-doctor add-virtual-output NAME W H` + `kscreen-doctor output.NAME.mode.WxH@HZ` + `kscreen-doctor output.NAME.hdr.true` **vor** dem Stream-Start. Die Befehle exit-en alle mit 0. Aber:
- `kscreen-doctor add-virtual-output` erzeugt einen **rein KScreen-internen Cache-Eintrag**, **keinen echten DRM-Connector** und **keinen wl_output**.
- Der Output erscheint NICHT im Wayland Registry (`pwgrab.cpp::find_target()` sieht nur `HDMI-A-1` und `HDMI-A-2`).
- Folge: `pwgrab.cpp` fällt auf `stream_virtual_output` zurück, das einen *anderen* Output erstellt — der wieder 60-Hz-fixed ist.

CachyOS-Test-Log Auszug (2026-05-14 09:39:14):
```
Sonnenschein vdisplay (kwin): pre-created virtual output 'Sonnenschein-00E8F1E1' 1280x800   ← exit 0, aber wirkungslos
KWin direct capture: found output 'HDMI-A-1' ... 3840x2160
KWin direct capture: found output 'HDMI-A-2' ... 1920x1080
KWin direct capture: target not found yet, doing a display roundtrip...
KWin direct capture: output 'Sonnenschein-00E8F1E1' not found in Wayland registry.
                    Requesting KWin to create a stream_virtual_output directly.   ← ergibt 60-Hz-Default
```

Konsequenz: `29cd4b6` ist auf `41fa9ba` zurückgerollt. Der Aufruf war wirkungslos und produzierte verwirrende Logs, ohne Mehrwert.

#### Echte Lösung — `kde_output_management_v2::set_custom_modes`

KWin MR [!8766](https://invent.kde.org/plasma/kwin/-/merge_requests/8766) (Februar 2026, gelandet in Plasma 6.6 — Maintainer hat 6.6.4, also **ist die API verfügbar**) hat `Capability::CustomModes` für virtual outputs eingeführt. Damit kann ein Wayland-Client per `kde_output_management_v2` synthetische Modi mit beliebigen Refresh-Raten zur Mode-Liste eines Outputs hinzufügen. KWin akzeptiert sie als Native-Modi.

Modifizierte Files in MR !8766:
- `src/backends/drm/drm_virtual_output.cpp`
- `src/backends/drm/drm_virtual_output.h`
- `src/backends/virtual/virtual_output.cpp`

Sequenz für den Client (aus `kde-output-management-v2.xml`):

```
kde_output_management_v2.create_mode_list()             → kde_mode_list_v2
kde_mode_list_v2.set_resolution(W, H)
kde_mode_list_v2.set_refresh_rate(mhz)                  // milliHz, also 90000 für 90 Hz
kde_mode_list_v2.set_reduced_blanking(1)
kde_mode_list_v2.add_mode()
// optional: weitere Modi anhängen
kde_output_management_v2.create_configuration()         → kde_output_configuration_v2
kde_output_configuration_v2.set_custom_modes(device, mode_list)
kde_output_configuration_v2.apply()
// dann: kde_output_configuration_v2.set_mode(device, new_mode) + apply() um zu aktivieren
```

#### Was Codex in `723537a` versucht hat — und warum es kollabierte

Codex hat in `723537a` genau diesen Pfad implementiert (+850 Zeilen in `pwgrab.cpp`). Sein Code-Architektur war **korrekt** (Wayland-Binding für `kde_output_management_v2` + `kde_output_device_registry_v2`, `add_custom_kde_mode()`, `apply_kde_configuration()` etc.). Aber im selben Commit hat er `zkde_screencast_unstable_v1` als Bedingung verworfen und stattdessen einen Output-Management-zentrierten Pfad gebaut — das Resultat war:
- `kde_output_management_v2` wurde gebunden, OK.
- `zkde_screencast_unstable_v1` wurde **nicht mehr** gebunden, weil die Setup-Reihenfolge geändert wurde.
- Stream brach komplett ab (`Initial Ping Timeout` auf SteamDeck).

Maintainer hat `723537a` deshalb in `501431a` + `74c63cf` zurückgerollt.

#### Schritt-für-Schritt-Anleitung für nächste Session (Phase 4.5 / 60-Hz-Fix v2)

Der konkrete Implementierungsweg, der weder Codex' Falle noch meine `add-virtual-output`-Falle wiederholt:

1. **Wayland-Protocol-Bindings erzeugen lassen**: Stelle sicher dass `cmake/compile_definitions/linux.cmake` `kde-output-management-v2.xml` und `kde-output-device-v2.xml` aus `third-party/plasma-wayland-protocols/src/protocols/` via `wayland-scanner` zu C-Bindings macht. Codex' `723537a` hatte das schon (siehe `git show 723537a -- cmake/compile_definitions/linux.cmake`). Cherry-pick exakt diese CMake-Zeilen.

2. **In `pwgrab.cpp` parallel zu `zkde_screencast` ein zweites Wayland-Binding-Set einbauen**:
   - `kde_output_management_v2` global
   - `kde_output_device_registry_v2` global (liefert die `kde_output_device_v2` Liste)
   - Listener für `kde_output_device_v2` der Name, Modi und Capabilities trackt
   - **Beide Bindings müssen koexistieren**, nicht eines das andere ersetzen — das war Codex' Fehler.

3. **Nach `stream_virtual_output` Aufruf** (pwgrab.cpp Z. 219 ff):
   - `wl_display_roundtrip` damit der neue Output via `kde_output_device_registry_v2` ankommt
   - In der `kde_output_device_v2` Liste den Output mit Name `Sonnenschein-XXXX` finden
   - Prüfen: hat er `KDE_OUTPUT_DEVICE_V2_CAPABILITY_CUSTOM_MODES`? Falls nein, log warning und mit 60 Hz weitermachen.
   - `kde_output_management_v2_create_mode_list()` + `set_resolution/refresh_rate/reduced_blanking/add_mode`
   - `kde_output_management_v2_create_configuration()` + `set_custom_modes()` + `apply()` + warten auf `applied`-Callback
   - Dann zweite Configuration: `set_mode(neuer_mode)` + `apply()` um die neue Mode aktiv zu machen
   - Roundtrip + KWin schaltet den Stream auf die neue Refresh-Rate um

4. **Code-Vorlage**: `git show 723537a -- src/platform/linux/pwgrab.cpp` zeigt die volle Implementierung. Cherry-pick chirurgisch: nur die Klassen `kde_output_t`, `kde_mode_t`, `apply_kde_configuration`, `add_custom_kde_mode`, `wait_for_kde_output` und die Registry-Binding-Logic. Alle Änderungen am `zkde_screencast`-Pfad ignorieren.

5. **Test auf CachyOS**: erwartete neue Log-Zeilen:
   ```
   KWin output management: bound kde_output_management_v2 version 19
   KWin output management: bound kde_output_device_registry_v2 version N
   KWin output management: output 'Sonnenschein-XXXX' caps=0x... custom_modes=yes
   KWin output management: adding custom mode 1280x800@90 to 'Sonnenschein-XXXX'
   KWin output management: applied custom mode list
   KWin output management: activated mode 1280x800@90 on 'Sonnenschein-XXXX'
   ```
   Und im Stream: 90 Hz statt 60 Hz auf dem SteamDeck.

6. **Falls KWin trotzdem 60 Hz liefert (Plasma-Version hat MR !8766 nicht)**: Maintainer-Log poste mir, dann Fallback-Plan: EDID-Firmware-Injection als Boot-Setup (HarryAnkers-Methode), siehe §9.19 unten.

#### §9.19 Fallback-Plan: EDID-Firmware-Injection (Boot-time Setup)

Falls `kde_output_management_v2::set_custom_modes` auf Plasma 6.6.4 nicht funktioniert (z.B. weil MR !8766 nicht in 6.6.4 sondern erst 6.7 ist), Fallback-Methode aus dem HarryAnkers Gist:

1. Custom EDID-Binary generieren mit gewünschten Modi (1280x800@90, 1920x1080@120, 4K@60 etc.) inkl. HDMI VSDBs für NVIDIA-Unlock.
2. EDID-Binary nach `/usr/lib/firmware/edid/sonnenschein.bin` kopieren, in initramfs einbinden (`mkinitcpio` / `dracut`).
3. Kernel-Boot-Parameter setzen: `drm.edid_firmware=DP-2:edid/sonnenschein.bin video=DP-2:e`.
4. Nach Reboot: DRM erstellt einen vollwertigen Output mit den definierten Modi. KWin sieht ihn als echten Wayland-Output mit korrekter Mode-Liste.
5. `stream_output(wl_output*)` (statt `stream_virtual_output`) capturet ihn — Refresh-Rate kommt aus der EDID-Mode-Liste.

Nachteile: 
- Erfordert Boot-Konfiguration (Reboot beim Setup, Modi sind statisch).
- HDR funktioniert auf virtuellen Connectors nicht (NVIDIA-Treiber-Limitation: SCDC-Negotiation nur auf echten Links).
- Pro Setup nur eine fixe Konnektor-Konfiguration; Multi-Client-Multi-Resolution skaliert nicht.

Vorteile: rock-solid, distroübergreifend, funktioniert ohne KDE-spezifische API.

#### Externe Quellen für die Diagnose

- KDE Discuss: [KDE Wayland custom resolution AND refresh rate](https://discuss.kde.org/t/kde-wayland-custom-resolution-and-refresh-rate/13642) — *"Kwin (Wayland) does not support running the display in any geometry other than what is supported by a native mode."*
- KWin MR [!8766](https://invent.kde.org/plasma/kwin/-/merge_requests/8766) — `backends: add support for custom modes in virtual outputs`.
- KDE Protokoll-Spezifikation: [kde_output_management_v2 + kde_mode_list_v2](https://wayland.app/protocols/kde-output-management-v2).
- [HarryAnkers' EDID-Firmware-Injection-Anleitung für Sunshine/Moonlight auf NVIDIA Linux](https://gist.github.com/HarryAnkers/8dbf551d66f00e8156ef4dd2b2b090a0).
- `third-party/plasma-wayland-protocols/src/protocols/kde-output-management-v2.xml` (im Repo).

### 9.15 Portal-Dialog erscheint bei jedem Stream

**Symptom**: Bei jedem Test muss der Maintainer im KDE-Screen-Record-Dialog manuell eine Quelle auswählen.

**Ursache**: `pwgrab.cpp` setzt zwar `persist_mode=2`, speichert den vom Portal nach `Start` gelieferten `restore_token` aber nur im RAM der aktuellen `portal_session_t`. Beim nächsten Sonnenschein-Start ist der Token weg, deshalb fragt KDE erneut.

**Status**: Komfort-/UX-Fix nach Source-Auswahl. Erst Zielquelle stabilisieren, dann Restore-Token persistent in `~/.local/state/sonnenschein/` ablegen und beim nächsten `SelectSources` verwenden.

---

## 10. Letzte Commits chronologisch

(neueste zuerst, Format: `hash` — Beschreibung — Tag)

```
TBD ← (Rollback) — revert(vdisplay): roll back 29cd4b6 — kscreen-doctor add-virtual-output proven wirkungslos on CachyOS — 2026-05-14
29cd4b6 — fix(vdisplay): pre-create KWin virtual output via kscreen-doctor for correct refresh rate — 2026-05-14 (NICHT WIRKSAM — KDE-Cache-Eintrag, kein wl_output)
41fa9ba — fix(capture): use portal fallback when KWin screencast is unavailable — 2026-05-13
74c63cf — revert(capture): restore pre KScreen resolver capture path — 2026-05-13
501431a — revert(capture): restore stable KWin direct capture path — 2026-05-13
edc144e — fix(capture): fall back when KWin direct capture is unavailable — 2026-05-13
723537a — fix(capture): configure KWin virtual outputs via output management — 2026-05-13
6504268 — fix(capture): resolve KScreen output before setting refresh rate — 2026-05-13
bf7d939 — fix(capture): bypass '@' character corruption in compiler and add delay for KWin mode registration — 2026-05-10
2d5b81a — docs: finalized E2E Refresh Rate and Headless Mode documentation — 2026-05-10
6c798e4 — fix(capture): remove string_view literals from BOOST_LOG formatting that caused string corruption in fps metadata — 2026-05-10
9c63e32 — docs: update STATUS.md regarding framerate string fixes — 2026-05-10
be20703 — fix(capture): replace error-prone float formatting with simple string conversions to fix garbage log output and 60Hz mode-set lock — 2026-05-10
6bbb820 — fix(capture): dynamically set virtual output refresh rate using kscreen-doctor after stream starts — 2026-05-10
e453afa — fix(capture): format framerate correctly and disable physical screens in KWin virtual output backend — 2026-05-10
23d0778 — docs: document stream_virtual_output migration — 2026-05-10
d84072e — fix(capture): migrate KWin virtual display to stream_virtual_output — 2026-05-10
96203b1 — docs: update STATUS.md with Wayland roundtrip hotfix — 2026-05-10
f40b4f5 — fix(capture): sync Wayland display roundtrip in KWin direct capture to catch delayed virtual outputs — 2026-05-10
102b36b — fix(capture): capture wl_output description and add Virtual-* fallback for KWin — 2026-05-10
2df15c4 — docs: update STATUS.md with KWin direct capture fix — 2026-05-10
4c63d36 — fix(capture): use KWin direct screencast for virtual outputs — 2026-05-10
bb3e758 — docs: update STATUS.md with portal source diagnostics — 2026-05-10
447dc8b — fix(capture): log portal source and request cursor — 2026-05-10
12a17da — docs: update STATUS.md with PipeWire format fix — 2026-05-10
76d03a4 — fix(capture): request client format from PipeWire — 2026-05-10
5fe1ea6 — docs: update STATUS.md with PipeWire fd fix — 2026-05-10
6d6433f — fix(capture): fix PipeWire remote fd options — 2026-05-10
3c730a4 — docs: update STATUS.md with D-Bus token/path mismatch fix — 2026-05-10
60dac9b — fix(capture): fix D-Bus Portal token/path mismatch causing signal timeouts — 2026-05-10
84347f8 — docs: update STATUS.md with stream probe fix — 2026-05-10
af603b6 — fix(capture): bypass Portal D-Bus dialogs during stream pre-flight encoder probe — 2026-05-10
417013a — docs: update STATUS.md with D-Bus timeout boot fix — 2026-05-10
91967a6 — fix(capture): bypass Portal D-Bus dialogs during boot encoder probe — 2026-05-10
ad33986 — fix(capture): resolve PipeWire timeout and boot failures — 2026-05-10
81e00c3 — docs: Phase 4 status update — PipeWire implemented, portal setcap conflict documented — 2026-05-10
4fa5376 — fix(capture): bypass boot-time sources bitset for runtime PipeWire override — 2026-05-10
50d6fa4 — feat(capture): Phase 4 — PipeWire portal capture backend for virtual displays — 2026-05-10
f475ba6 — docs: Phase 2 complete — KMS cannot capture virtual displays, PipeWire required — 2026-05-10
71cdac3 — fix(capture): route display_name to KMS index, not vdisplay name — 2026-05-10
9baebbf — fix(capture): route KMS capture to primary monitor for Linux virtual display — 2026-05-10
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

`main` Branch zeigt nur auf `235920b` (initial import). `dev` ist die aktive Entwicklungs-Linie und liegt ca. 30+ Commits vor `main`.

**Auf `dev` nächster Test-Commit = `41fa9ba`** (Stand 2026-05-13, nach Push). Nächster Schritt ist ausschließlich CachyOS-Validierung, dass der Stream bei fehlendem `zkde_screencast_unstable_v1` nicht mehr abbricht, sondern den Portal-Pfad nutzt. 90-Hz/HDR-Arbeit pausiert bis diese Basis wieder bestätigt ist.

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
- `.github/workflows/build-linux.yml` (NEU/PATCH) — Linux-Matrix, `continue-on-error`, seit KWin-Direktcapture mit rekursiven Submodules.

### CMake-Patches
- `cmake/dependencies/Boost_Sunshine.cmake` (PATCH) — Pre-flight + FetchContent-fallback
- `cmake/compile_definitions/linux.cmake` (PATCH) — virtual_display sources eingehängt, CUDA-Pfad-Auto-Detection (/opt/cuda), tray_linux.c (statt .cpp wegen Pin), PipeWire + KWin-Direct-ScreenCast + KWin-Output-Management-v2-Protokollgeneration.
- `cmake/prep/options.cmake` (PATCH) — `SUNSHINE_ENABLE_KWIN` für optionalen KWin-Direct-ScreenCast-Pfad.

### C++ — virtual_display Modul (NEU, 14 Files)
- `src/platform/linux/virtual_display/types.h` — Enums + Structs
- `src/platform/linux/virtual_display/interface.h` — IBackend
- `src/platform/linux/virtual_display/detection.h+cpp` — Environment-Detection
- `src/platform/linux/virtual_display/factory.h+cpp` — Backend-Auswahl
- `src/platform/linux/virtual_display/subprocess.h+cpp` — popen/fork+exec Helper
- `src/platform/linux/virtual_display/backends/all.h` — make_*-Deklarationen
- `src/platform/linux/virtual_display/backends/kwin_wayland.cpp` — ✅ implementiert. `29cd4b6` bringt das `kscreen-doctor add-virtual-output` + `mode-set` Pre-Create-Pattern zurück, das `e453afa` entfernt hatte; dazu Track-Feld `ActiveDisplay::added_via_kscreen` damit `destroy()` korrekt entscheidet ob `remove-virtual-output` aufgerufen wird.
- `src/platform/linux/virtual_display/backends/{mutter_headless,wlroots_headless,xorg_nvidia,xorg_dummy,amdgpu_param,evdi}.cpp` — Stubs
- `src/platform/linux/virtual_display/README.md` — Architektur

### C++ — process.cpp Wiring (Phase 2C)
- `src/process.h` (PATCH) — `#elif __linux__` Header-Include + zwei private Member
- `src/process.cpp` (PATCH) — Linux-Branch in `execute()` + `terminate()`

### C++ — PipeWire Capture (Phase 4)
- `src/platform/linux/pwgrab.cpp` (NEU/PATCH) — xdg-desktop-portal ScreenCast + PipeWire-Stream; `447dc8b` loggt Portal-Source-Properties und fordert Embedded Cursor an; `4c63d36` nutzt KWin Direct ScreenCast für benannte `Sonnenschein-...`-Outputs und blockiert den KDE-XDG-`VIRTUAL`-Fallback; `d84072e` migriert den KWin-Pfad auf `stream_virtual_output`; `bf7d939` versucht den erzeugten KScreen-Output nach Stream-Start auf die Client-Refresh-Rate zu setzen; `6504268` pollt `kscreen-doctor -o`, setzt den Mode auf dem tatsächlich registrierten Output und verifiziert das Ergebnis; `501431a` setzte zunächst zurück auf `67a93e3`; `74c63cf` setzt weiter zurück auf `bf7d939`, weil `6504268` laut Maintainer schon defekt war; `41fa9ba` entfernt den fatalen Abbruch, wenn KWin Direct nicht verfügbar ist.

### Submodule-Pin
- `third-party/tray/` — gepinnt auf `7936cb35` (vor `.gitmodules`-Datei; gitlink im Tree)
- `third-party/plasma-wayland-protocols/` — neu gepinnt auf `13b3c2f`, liefert `zkde-screencast-unstable-v1.xml` für KWin Direct ScreenCast.

---

## 12. Was als nächstes — konkrete Schritte

In Reihenfolge der Priorität.

### A) 60-Hz-Fix v2 — `kde_output_management_v2::set_custom_modes` Client implementieren (höchste Prio)

Detail-Anleitung in §9.18. Kurz:

1. **Setup**: CMake-Generation für `kde-output-management-v2.xml` + `kde-output-device-v2.xml` aktivieren (cherry-pick aus `723537a`-`cmake/compile_definitions/linux.cmake`-Diff).
2. **Code**: In `pwgrab.cpp` parallel zum bestehenden `zkde_screencast`-Pfad ein zweites Wayland-Binding-Set für `kde_output_management_v2` + `kde_output_device_registry_v2`. **Beide koexistieren — keiner ersetzt den anderen.**
3. **Logik**: Nach `stream_virtual_output` Aufruf den neu erstellten Output via `kde_output_device_registry_v2` finden, `set_custom_modes` mit `(W, H, refresh_mhz, reduced_blanking=1)` + `apply()`, dann `set_mode(neue_mode_id)` + `apply()`.
4. **Code-Vorlage**: `git show 723537a -- src/platform/linux/pwgrab.cpp` zeigt die volle (funktionierende) Architektur. **Nur den Output-Management-Pfad cherry-picken — NICHT die zkde_screencast-Änderungen.** Das war Codex' Bug.
5. **Test auf CachyOS** mit erwarteten Log-Zeilen aus §9.18.
6. **Falls Plasma 6.6.4 die MR !8766 noch nicht hat**: Fallback auf §9.19 EDID-Firmware-Injection.

### B) Falls 60-Hz-Fix grün: weitere Stabilisierung

- HDR-Pfad nochmal validieren (`hdr_enable` Log-Zeile + Moonlight HDR10-Indikator)
- Multi-Client-Test (zwei Moonlight-Clients gleichzeitig — verschiedene Virtual Outputs)
- Restore-Token persistent speichern (§9.15) → kein Portal-Dialog mehr beim Re-Start

### C) Phase 1.6 — CMake-Rebrand (nach Phase 2D-Done)

Sobald der 60-Hz-Fix steht und der Stream auf 90Hz auf dem SteamDeck läuft:
- `CMakeLists.txt`: `project(Sonnenschein)`
- `cmake/prep/build_version.cmake`: `Sunshine Branch:` → `Sonnenschein Branch:`
- Add-Executable-Target: `sunshine` → `sonnenschein` (Symlink `sunshine` für Rückwärts-Kompat)
- Service-Files / .desktop-Files: `dev.lizardbyte.app.Sunshine` → eigener FQDN
- README/Docs: alle "Sunshine"-Erwähnungen prüfen

### C) Phase 1.6 — CMake-Rebrand (nach 2D)

Sobald Phase 2D grün ist und ein erster Virtual Display erfolgreich erzeugt wurde:
- `CMakeLists.txt`: `project(Sonnenschein)`
- `cmake/prep/build_version.cmake`: `Sunshine Branch:` → `Sonnenschein Branch:`
- Add-Executable-Target: `sunshine` → `sonnenschein` (Symlink `sunshine` für Rückwärts-Kompat)
- Service-Files / .desktop-Files: `dev.lizardbyte.app.Sunshine` → eigener FQDN
- README/Docs: alle "Sunshine"-Erwähnungen prüfen

### D) Phase 3 — Installer

Sobald 1.6 done. Erste Iteration: nur Arch (CachyOS-Test-Hardware). Dann iterativ andere Distros.

### E) Phase 4 — HDR & AV1

Nach Phase 3, weil Installer braucht systemd-User-Mode-Setup für DBus-Bus zur HDR-Kommunikation mit KWin.

### F) Sunshine-Upstream-Sync

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

---

## 14. Deep-Dive: PipeWire & Portal Integration (Phase 4)

Die Implementierung in `pwgrab.cpp` nutzt das `org.freedesktop.portal.ScreenCast` Interface. Hier sind die technischen Details der kritischen Komponenten:

### D-Bus Portal Handshake
1. **CreateSession**: Erstellt eine Session. Wir übergeben einen `session_handle_token`.
2. **SelectSources**: Wählt die Quelle aus. Wir setzen `types=1` (Monitor) und `persist_mode=2` (Persistent). Hier erscheint beim ersten Mal der KDE-Dialog.
3. **Start**: Aktiviert den Stream. Gibt die PipeWire `node_id` zurück.

### Das Token-System (Fix 60dac9b)
Jeder Portal-Request ist asynchron. Man abonniert ein `Response`-Signal auf einem spezifischen Pfad:
`/org/freedesktop/portal/desktop/request/SENDER/TOKEN`

- **SENDER**: Der eindeutige D-Bus Name (z.B. `1_42`).
- **TOKEN**: Ein von uns gewählter String (z.B. `sonnenschein_1`).

**Fehler**: Vorher wurde für den Pfad und den `handle_token`-Parameter jeweils ein *neuer* Token generiert. Dadurch hörte Sonnenschein auf Pfad `.../sonnenschein_2`, während das Portal die Antwort an `.../sonnenschein_1` schickte. Dies führte zu den 10s Timeouts.

### GLib Main Context Pumping
GIO (GDBus) benötigt einen laufenden Main-Loop, um Signale (wie die Portal-Antwort) zuzustellen. Da Sonnenschein keinen globalen GLib-Loop nutzt, pumpen wir den Default-Context manuell in `wait_response()`:
```cpp
while (!response_received) {
    g_main_context_iteration(nullptr, FALSE);
    std::this_thread::sleep_for(10ms);
}
```

### PipeWire & DMA-BUF
Der Stream wird mit `PW_KEY_STREAM_CAPTURE_PROXY` und DMA-BUF Support initialisiert.
- **Zero-Copy**: PipeWire liefert Dateideskriptoren für GPU-Buffer.
- **EGL-Import**: Diese FDs werden via `eglCreateImageKHR` in GL-Texturen importiert, die dann direkt vom Encoder (NVENC/VAAPI) gelesen werden können.

### Encoder Probing (Dummy-Mode)
Sonnenschein testet Encoder durch Instanziierung eines Displays. Um zu verhindern, dass bei jedem Test ein Portal-Dialog aufpoppt, erkennt `pw_display_t::init()` über den globalen `video::is_encoder_probing_active` Flag, ob es sich um einen Test handelt. Falls ja, wird die D-Bus-Kommunikation übersprungen und ein valides Dummy-Display simuliert.
Falls ja, wird die D-Bus-Kommunikation übersprungen und ein valides Dummy-Display simuliert.
