# Building Sonnenschein

Sonnenschein is **Linux-only**. This guide covers building from source on the supported distros plus the WSL2-on-Windows dev path.

> **Pre-Alpha**: many CMake paths and submodule pins still reflect Apollo conventions. Expect rough edges until Phase 1 wraps. See [docs/ROADMAP.md](ROADMAP.md).

## TL;DR

```bash
git clone --recursive https://github.com/Elias02345/sonnenschein.git
cd sonnenschein

# Distro-specific dependency install — see "Dependencies" below.
# Then:
mkdir -p build && cd build
cmake -G Ninja -S .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DSUNSHINE_BUILD_DOCS=OFF \
    -DSUNSHINE_BUILD_HOMEBREW=OFF \
    -DSUNSHINE_BUILD_FLATPAK=OFF \
    -DSUNSHINE_ENABLE_CUDA=OFF   # remove this if you have CUDA installed
cmake --build . --target sunshine -j$(nproc)
./sunshine --help
```

(The binary is currently still named `sunshine`. The `Sonnenschein` rename happens in Phase 1.6.)

## Toolchain requirements

| Tool | Minimum | Notes |
|---|---|---|
| CMake | 3.25 | Apollo's lower bound; we keep it |
| GCC or Clang | GCC 13 / Clang 17 | C++23 features in use |
| Ninja | any recent | Optional but recommended |
| Node.js | **20.19+** or 22.12+ | Vite 6 + `@vitejs/plugin-vue` 6 use `crypto.hash` (Node 18 fails) |
| pkg-config | any | |
| libva | **API 1.21+** (lib 2.22+) | Needed by `build-deps`' pre-compiled FFmpeg (`vaMapBuffer2`) |
| Optional: CUDA Toolkit | 12.x | Only for NVIDIA NVENC. Skip if you don't have an NVIDIA GPU |

## Dependencies

### Arch / CachyOS / EndeavourOS / Manjaro

```bash
sudo pacman -Syu --needed \
    base-devel cmake ninja git nodejs npm \
    boost ffmpeg openssl curl miniupnpc opus \
    libdrm libva libpipewire libevdev libnotify libcap \
    vulkan-icd-loader vulkan-headers \
    wayland wayland-protocols \
    libx11 libxcb libxfixes libxrandr libxtst \
    libappindicator-gtk3
```

### Ubuntu 24.04 / Debian 12+

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config git wget curl ca-certificates \
    desktop-file-utils libcap-dev libcurl4-openssl-dev libdrm-dev libevdev-dev \
    libgbm-dev libminiupnpc-dev libnotify-dev libnuma-dev libopus-dev \
    libpulse-dev libssl-dev libwayland-dev libx11-dev libxcb-shm0-dev \
    libxcb-xfixes0-dev libxcb1-dev libxfixes-dev libxrandr-dev libxtst-dev \
    libva-dev libpipewire-0.3-dev libvulkan-dev udev xvfb libappindicator3-dev

# Node 20 (Apollo's required Node 18 from Ubuntu apt is too old for Vite 6)
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo bash -
sudo apt-get install -y nodejs

# Ubuntu 24.04 stock libva is 2.20 (API 1.20). Sonnenschein's pre-built
# FFmpeg needs API 1.21+. Build a newer libva:
sudo apt-get install -y meson python3-mako libxcb-dri3-dev libxcb-present-dev \
    libxcb-sync-dev libxcb-xfixes0-dev libx11-xcb-dev
git clone --depth 1 -b 2.22.0 https://github.com/intel/libva.git /tmp/libva
cd /tmp/libva
meson setup build --prefix=/usr --libdir=lib/x86_64-linux-gnu --buildtype=release \
    -Dwith_x11=yes -Dwith_wayland=yes -Dwith_glx=yes
sudo ninja -C build install
```

### Fedora 41+ / Nobara

```bash
sudo dnf install -y \
    gcc-c++ cmake ninja-build pkgconfig git nodejs npm \
    boost-devel openssl-devel libcurl-devel miniupnpc-devel opus-devel \
    libdrm-devel libva-devel pipewire-devel libevdev-devel libnotify-devel libcap-devel \
    vulkan-loader-devel vulkan-headers \
    wayland-devel wayland-protocols-devel \
    libX11-devel libxcb-devel libXfixes-devel libXrandr-devel libXtst-devel \
    libappindicator-gtk3-devel pulseaudio-libs-devel

# RPM Fusion for ffmpeg-full (replaces Fedora's ffmpeg-free)
sudo dnf install -y https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
sudo dnf swap ffmpeg-free ffmpeg-full --allowerasing
```

### openSUSE Tumbleweed

```bash
sudo zypper install -y \
    gcc-c++ cmake ninja git nodejs20 npm20 pkg-config \
    boost-devel libopenssl-devel libcurl-devel miniupnpc-devel libopus-devel \
    libdrm-devel libva-devel pipewire-devel libevdev-devel libnotify-devel libcap-devel \
    vulkan-devel \
    wayland-devel wayland-protocols-devel \
    libX11-devel libxcb-devel libXfixes-devel libXrandr-devel libXtst-devel \
    libappindicator3-devel pulseaudio-devel
```

## CUDA / NVIDIA NVENC

Only needed if you want hardware encoding on NVIDIA GPUs.

```bash
# Arch:    sudo pacman -S cuda
# Fedora:  sudo dnf install cuda
# Ubuntu:  https://developer.nvidia.com/cuda-downloads (use the .deb (network) installer)
```

If CUDA is not installed, configure with `-DSUNSHINE_ENABLE_CUDA=OFF`. You can still build everything else; software encoding (libx264, libx265, SvtAv1) and VAAPI will still work.

## Configure & build

```bash
mkdir -p build && cd build
cmake -G Ninja -S .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

A successful build produces `build/sunshine` (~33 MB ELF). The WebUI is built into `build/assets/web/` by the `web-ui` CMake target.

## WSL2 dev workflow (on Windows hosts)

The maintainer develops the Windows side via WSL2 Ubuntu 24.04. To replicate:

```powershell
# Once on Windows:
wsl --install -d Ubuntu-24.04
```

Then in WSL:

```bash
# As root inside WSL (since the WSL default user has password sudo):
# Install all deps as listed under "Ubuntu 24.04 / Debian 12+" above.

# Source path: /mnt/c/Users/<you>/Documents/ClaudeCode/sonnenschein
# Build path:  /tmp/snsbuild  (use Linux-native FS for build dir — much faster than /mnt/c)

mkdir -p /tmp/snsbuild
cd /tmp/snsbuild
cmake -S /mnt/c/Users/<you>/Documents/ClaudeCode/sonnenschein \
    -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
    -DSUNSHINE_ENABLE_CUDA=OFF
cmake --build . --target sunshine -j$(nproc)
```

Times observed on WSL2 (8-core, /mnt/c source + native build dir):
- cmake configure: ~6 min (cold cache, much of it Boost+FetchContent)
- cmake --build: ~10–15 min (261 build steps)
- npm run build (WebUI): ~2 min

## Submodules

Run `git submodule update --init --recursive` after every pull. The `third-party/build-deps` submodule is large (~1.1 GB; pre-built FFmpeg + Boost binaries for all platforms — we only use Linux-x86_64 but get the rest along for the ride for now).

## Useful CMake flags

| Flag | Default | Description |
|---|---|---|
| `BUILD_TESTS` | ON | Compile gtest suite |
| `SUNSHINE_BUILD_DOCS` | ON | Build Doxygen docs (needs `doxygen` + `graphviz`) |
| `SUNSHINE_ENABLE_CUDA` | ON | NVIDIA NVENC support; requires CUDA Toolkit |
| `SUNSHINE_ENABLE_VAAPI` | ON | AMD/Intel VAAPI hardware encoding |
| `SUNSHINE_ENABLE_DRM` | ON | KMS direct capture (needs `cap_sys_admin` at runtime) |
| `SUNSHINE_ENABLE_WAYLAND` | ON | Wayland capture (wlroots screencopy + KWin + portal) |
| `SUNSHINE_ENABLE_X11` | ON | Xorg capture |
| `SUNSHINE_ENABLE_VULKAN` | ON | Vulkan Video Encode (experimental) |
| `SUNSHINE_BUILD_FLATPAK` | OFF | Adjust paths for Flatpak packaging |
| `SUNSHINE_BUILD_HOMEBREW` | OFF | macOS-only; ignore on Linux |

## Troubleshooting

### `undefined reference to vaMapBuffer2`
Your distro's libva is older than API 1.21. Either:
- Upgrade libva from your distro's bleeding-edge / staging repo
- Build libva 2.22+ from source (see Ubuntu section)
- Configure with `-DSUNSHINE_ENABLE_VAAPI=OFF` (loses AMD/Intel hardware encoding)

### `crypto.hash is not a function` during `npm run build`
Node.js < 20.19. Upgrade Node (see Ubuntu instructions).

### `tray_linux.c not found`
You're on a `tray` submodule HEAD that has migrated to Qt (`tray_linux.cpp`). Ensure the submodule is at `7936cb3` (the pre-Qt commit Sonnenschein pins). `git submodule update --init --recursive --force` should fix it.

### CMake errors about `moonlight-common-c/enet` missing CMakeLists
You forgot the `--recursive` on submodule init. Run:
```bash
git submodule update --init --recursive
```

### CUDA configure error
Either install CUDA Toolkit or set `-DSUNSHINE_ENABLE_CUDA=OFF`.

## Where to file bug reports

- Build issues: [Issues → Bug template](https://github.com/Elias02345/sonnenschein/issues/new?template=bug.yml)
- Discussion / setup help: [Discussions](https://github.com/Elias02345/sonnenschein/discussions)
