<div align="center">

# Sonnenschein

**Linux-Game-Streaming, das einfach funktioniert.**

*High-quality, low-latency game streaming aus deinem Linux-PC oder -Server zu jedem Moonlight-fähigen Gerät — mit dynamisch erzeugten virtuellen Displays in der exakten Auflösung und Bildwiederholrate des Clients.*

[![Status](https://img.shields.io/badge/status-pre--alpha-orange?style=for-the-badge)](#projektstatus)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue?style=for-the-badge)](LICENSE)
[![Plattform](https://img.shields.io/badge/Linux-Wayland%20%7C%20X11-success?style=for-the-badge&logo=linux)](#unterst%C3%BCtzte-distros)
[![GPUs](https://img.shields.io/badge/GPUs-NVIDIA%20%7C%20AMD%20%7C%20Intel-76b900?style=for-the-badge)](#gpu-support)

[Deutsch](#deutsch) · [English](#english) · [Roadmap](docs/ROADMAP.md) · [Diskussionen](https://github.com/Elias02345/sonnenschein/discussions) · [Issues](https://github.com/Elias02345/sonnenschein/issues)

</div>

---

## Deutsch

### Was ist Sonnenschein?

Sonnenschein ist ein **Linux-natives, headless Moonlight-Streaming-Backend**. Es macht aus deinem Linux-Rechner einen Cloud-Gaming-Host, der Spiele in voller GPU-Qualität an SteamDeck, TV, Handy, Tablet, Laptop oder einen anderen PC streamt — drinnen im LAN oder über das Internet.

**Der Trick**: Beim Pairing erkennt Sonnenschein die Auflösung, Bildwiederholrate und (optional) HDR-Fähigkeit deines Clients und erzeugt automatisch ein passendes virtuelles Display. Restliche Monitore werden bei Bedarf abgeschaltet, sodass die volle GPU-Leistung in das gestreamte Bild fließt.

### Highlights

- **Auto-Display-Match** — Pairing erstellt ein Virtual Display in der exakten Client-Charakteristik (Auflösung, Refresh, HDR).
- **GPU-First** — voller Ressourcen-Boost für das gestreamte Spiel, andere Outputs deaktivierbar.
- **Wayland-First** — KDE Plasma 6, GNOME, Sway, Hyprland — und X11 als Fallback.
- **Multi-Backend Virtual Display** — `amdgpu.virtual_display`, Mutter `--headless`, KWin Virtual Outputs, wlroots, Xorg-Dummy oder EVDI; je nach System und GPU wird automatisch der beste Pfad gewählt.
- **HDR10** — über NVENC HEVC Main10 / AV1 Main10 oder VAAPI; auf NVIDIA mit Treiber ≥ 580.94.11 (Wayland), auf AMD mit Mesa 25+.
- **Adaptive Codec-Wahl** — AV1 wenn möglich, sonst HEVC, sonst H.264. Bitrate passt sich Netzwerk und Latenz an.
- **NVIDIA, AMD, Intel** — alle drei mit hardware-beschleunigtem Encoding (NVENC, VAAPI, Quick Sync).
- **Distroübergreifender Installer** — ein Bash-Skript erkennt deine Distro und richtet alles ein. Arch, CachyOS, SteamOS, Fedora, Nobara, Ubuntu, Debian, ZorinOS, openSUSE.
- **systemd-Service** — startet automatisch im Hintergrund, kein Terminal nötig.
- **Web-UI** — modernes Vue 3 + PrimeVue 4 Frontend, Setup-Wizard, Live-Diagnose, Update-Knopf, Crash-Report-Submission, deutsch + englisch.
- **Auto-Update** — eingebauter Updater zieht neue Releases von GitHub. Branch wählbar (`main` stabil, `dev` aktuell).
- **Sauberer Uninstaller** — entfernt alles spurlos, falls gewünscht.
- **Open Source, GPL-3.0** — wie seine Vorbilder Sunshine und Apollo.

### Schnellinstallation

> Pre-Alpha — noch nicht für Nicht-Entwickler. Erste lauffähige Version folgt.

```bash
# Erscheint mit dem ersten Release:
curl -fsSL https://raw.githubusercontent.com/Elias02345/sonnenschein/main/installer/install.sh | bash
```

Der Installer fragt dich durch ein Text-UI nach Distro-Bestätigung, GPU-Wahl, Service-Modus (System/User), Auto-Start (ja/nein) und WebUI-Passwort. Nach 1–3 Minuten ist die WebUI auf `http://<dein-rechner>:47990` erreichbar.

### Unterstützte Distros

| Distro | Mindest-Version | Besonderheiten |
|---|---|---|
| Arch / CachyOS / EndeavourOS | rolling | Empfohlen für Gaming-Hosts |
| SteamOS 3+ | aktuell | Auf eigene Gefahr (read-only `/usr`) |
| Fedora / Nobara | 40+ | RPMFusion erforderlich für `ffmpeg-full` |
| Ubuntu | 22.04+ | HWE-Kernel empfohlen für NVIDIA 580+ |
| Debian | 12+ | Backports empfohlen |
| openSUSE Tumbleweed | aktuell | |
| ZorinOS | 17+ | (Ubuntu-basiert) |

### GPU-Support

| Vendor | Mindest-Hardware | Empfohlen | HDR |
|---|---|---|---|
| NVIDIA | Pascal (GTX 10xx) + Driver 550 | RTX 30/40-Serie + Driver 580+ | RTX 30/40 + Driver 580+ + Wayland |
| AMD | RDNA1 (RX 5xxx) | RDNA3+ (RX 7xxx) | RDNA2+ + Mesa 25+ |
| Intel | Skylake (Iris) für H.264/HEVC | Arc Battlemage für AV1 | experimentell |

### Compositors

| Compositor | Unterstützung | Backend |
|---|---|---|
| KWin (Plasma 6.2+) Wayland | **Erste Klasse** (Hauptfokus) | KWin Virtual Output / kscreen-doctor |
| GNOME (Mutter) Wayland | Erste Klasse | `mutter --headless --virtual-monitor` |
| Sway / wlroots | Erste Klasse | `WLR_BACKENDS=headless` |
| Hyprland | Best-Effort | wlroots-fork |
| Xorg | Fallback | xf86-video-dummy / NVIDIA EDID-Spoof |

### Wie es sich von ähnlichen Projekten unterscheidet

| Projekt | Sonnenschein bringt zusätzlich |
|---|---|
| **Sunshine** | Auto-Match Virtual Display auf Linux, Wayland-HDR, distroübergreifender Installer |
| **Apollo** | Linux-Port des SudoVDA-Killer-Features, Wayland-First |
| **Wolf** | Native Installation statt Container, einfaches Setup |
| **Moonshine** | WebUI, HDR, Auto-Display-Erstellung |

### Architektur in einem Diagramm

```
            ┌──────────────────────────────────────┐
 Moonlight  │  Pairing → ResExchange → RTSP/UDP    │  Sonnenschein-Server (C++)
   Client → │  Capabilities (HDR? AV1? Auflösung?) │  ├─ Virtual-Display-Abstraktion
            └──────────────────────────────────────┘  │   (Mutter / KWin / AMDGPU /
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

### Projektstatus

| Phase | Status | Beschreibung |
|---|---|---|
| 0. Vorbereitung | ✅ fertig | Repo, Branding, Lizenz, CI-Skelett |
| 1. Linux-Build | ✅ fertig | Apollo-Codebasis baut auf Linux (270/270 Tests) |
| 1.6 Rebrand | ✅ fertig | Apollo/Sunshine → Sonnenschein (Binary + Kompat-Symlink) |
| 2. Virtual Display | 🟡 in Arbeit | Multi-Backend-Abstraktion (KWin); 60-Hz/HDR CachyOS-Test offen |
| 3. Installer | 🟡 Gerüst | Distro-Detection, systemd, Uninstall (Distro-Runs offen) |
| 4. HDR & AV1 | offen | Codec-Caps + Compositor-HDR |
| 5. WebUI v1 | 🟡 Fundament | PrimeVue 4 + Diagnostics (Migration offen) |
| 6. Updates | offen | CLI + WebUI |
| 7. Tests & Doku | offen | |
| 8. Release 1.0 | offen | AUR, COPR, Flatpak |

Detaillierte Roadmap in [docs/ROADMAP.md](docs/ROADMAP.md).

### Mitmachen

Pull Requests willkommen — siehe [CONTRIBUTING.md](.github/CONTRIBUTING.md). Für Bugs und Feature-Requests bitte [Issues](https://github.com/Elias02345/sonnenschein/issues) nutzen.

### Lizenz & Anerkennungen

Sonnenschein ist GPL-3.0-only (siehe [LICENSE](LICENSE)) — wie seine Vorlagen [Sunshine](https://github.com/LizardByte/Sunshine) und [Apollo](https://github.com/ClassicOldSong/Apollo). Setup-Inspiration auch von [docker-steam-headless](https://github.com/josh5/docker-steam-headless). Volle Attribution in [NOTICE](NOTICE).

---

## English

### What is Sonnenschein?

Sonnenschein is a **Linux-native, headless Moonlight streaming backend**. It turns your Linux PC or server into a cloud-gaming host that streams games in full GPU quality to a Steam Deck, TV, phone, tablet, laptop, or any other Moonlight-compatible client — over LAN or the internet.

**The trick**: When a client pairs, Sonnenschein detects its resolution, refresh rate, and (optionally) HDR capability, then automatically spawns a matching virtual display. Other monitors can be turned off so the full GPU horsepower flows into the streamed image.

### Highlights

- **Auto display match** — pairing creates a virtual display in the exact client characteristics (resolution, refresh, HDR).
- **GPU first** — full resource boost for the streamed game, other outputs disable-able.
- **Wayland first** — KDE Plasma 6, GNOME, Sway, Hyprland — and X11 as a fallback.
- **Multi-backend virtual display** — `amdgpu.virtual_display`, Mutter `--headless`, KWin Virtual Outputs, wlroots, Xorg-dummy, or EVDI; the best path is auto-selected.
- **HDR10** — via NVENC HEVC Main10 / AV1 Main10, or VAAPI; on NVIDIA with driver ≥ 580.94.11 (Wayland), on AMD with Mesa 25+.
- **Adaptive codec choice** — AV1 if possible, then HEVC, then H.264. Bitrate adapts to network and latency.
- **NVIDIA, AMD, Intel** — all with hardware-accelerated encoding.
- **Cross-distro installer** — one bash script detects your distro and sets everything up. Arch, CachyOS, SteamOS, Fedora, Nobara, Ubuntu, Debian, ZorinOS, openSUSE.
- **systemd service** — auto-starts in the background, no terminal needed.
- **Web UI** — modern Vue 3 + PrimeVue 4 frontend; setup wizard, live diagnostics, update button, crash-report submission, German + English.
- **Auto-update** — built-in updater pulls new releases from GitHub. Branch is selectable (`main` stable, `dev` bleeding edge).
- **Clean uninstaller** — removes everything if you ever want it gone.
- **Open Source, GPL-3.0** — like its forefathers Sunshine and Apollo.

### Quick install

> Pre-alpha — not yet ready for non-developers. First runnable version coming soon.

```bash
# Will be available with the first release:
curl -fsSL https://raw.githubusercontent.com/Elias02345/sonnenschein/main/installer/install.sh | bash
```

The installer walks you through a text UI: distro confirmation, GPU choice, service mode (system/user), autostart (yes/no), and WebUI password. After 1–3 minutes the WebUI is reachable at `http://<your-host>:47990`.

### Project status

Pre-alpha. See [docs/ROADMAP.md](docs/ROADMAP.md) for the detailed phase plan.

### Contributing

Pull requests welcome — see [CONTRIBUTING.md](.github/CONTRIBUTING.md). For bugs and feature requests, use [Issues](https://github.com/Elias02345/sonnenschein/issues).

### License & credits

Sonnenschein is GPL-3.0-only (see [LICENSE](LICENSE)) — same as its parents [Sunshine](https://github.com/LizardByte/Sunshine) and [Apollo](https://github.com/ClassicOldSong/Apollo). Setup inspiration also from [docker-steam-headless](https://github.com/josh5/docker-steam-headless). Full attribution in [NOTICE](NOTICE).
