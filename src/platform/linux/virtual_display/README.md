# Sonnenschein — Linux Virtual Display Abstraction

This module is the heart of Sonnenschein's value proposition: when a Moonlight client pairs, we create a virtual display that exactly matches the client's resolution, refresh rate, and HDR capability — then point the streaming pipeline at it.

On Apollo this works only on Windows (via the SudoVDA driver). On Linux there's no single mechanism that covers every distro, every compositor, every GPU vendor — so this module abstracts the choice behind a single interface and selects the right backend at runtime.

## File map

```
virtual_display/
├── types.h                 — DisplayServer, Compositor, GpuVendor, GpuInfo,
│                             Environment, CreateRequest, Handle
├── interface.h             — IBackend (the contract every backend implements)
├── detection.h / .cpp      — detect()  → Environment
├── factory.h / .cpp        — select_backend(env, cfg)  → unique_ptr<IBackend>
├── backends/
│   ├── all.h               — make_*() factory function declarations
│   ├── kwin_wayland.cpp    — KDE Plasma 6+ Wayland (Phase 2B target)
│   ├── mutter_headless.cpp — GNOME Wayland
│   ├── wlroots_headless.cpp— Sway / Hyprland
│   ├── xorg_nvidia.cpp     — Xorg + NVIDIA (proprietary or open kernel)
│   ├── xorg_dummy.cpp      — Xorg + xf86-video-dummy (AMD/Intel SW fallback)
│   ├── amdgpu_param.cpp    — amdgpu.virtual_display kernel parameter
│   └── evdi.cpp            — EVDI DKMS module (universal last-resort)
└── README.md (this file)
```

## How selection works

1. `detection::detect()` returns an `Environment` describing the running system: display server (Wayland/Xorg), compositor + version, list of GPUs, and a heuristic primary-GPU pick (NVIDIA > AMD > Intel).

2. `factory::select_backend(env, cfg)` consults `cfg.preferred_backend`:
   - `""` or `"auto"` → walk the priority list, return the first backend whose `available(env)` is true.
   - explicit name (e.g. `"kwin_wayland"`) → return that backend even if it reports unavailable, with a warning.

3. Backend priority order (top of `factory.cpp`):
   `kwin_wayland → mutter_headless → wlroots_headless → xorg_nvidia → xorg_dummy → amdgpu_param → evdi`.

4. If nothing matches, `select_backend` returns `nullptr` and the caller (eventually `process.cpp`) falls back to streaming the existing primary display.

## Backend status (Phase 2A: foundation only)

| Backend | `available()` | `create()` | Phase 2 milestone |
|---|---|---|---|
| `kwin_wayland` | Returns true on Plasma 6+ Wayland | Stub (returns nullopt) | **2B — primary target** |
| `mutter_headless` | True on GNOME Wayland | Stub | 2C |
| `wlroots_headless` | True on Sway / Hyprland | Stub | 2C |
| `xorg_nvidia` | True on Xorg + NVIDIA GPU | Stub | 2D |
| `xorg_dummy` | True on Xorg + AMD or Intel | Stub | 2E |
| `amdgpu_param` | True if any AMD GPU present | Stub | 2E |
| `evdi` | Always false (last-resort, manual only) | Stub | 2F |

Each stub class header in the corresponding `.cpp` includes a `Plan:` block describing exactly how the real backend will work. Look there before starting a 2B/2C/2D/... PR.

## Configuration (sonnenschein.conf)

```ini
[virtual_display]
# auto | kwin_wayland | mutter_headless | wlroots_headless |
# xorg_nvidia | xorg_dummy | amdgpu_param | evdi
backend = auto

# auto | <pci-bus-id like 0000:01:00.0>
gpu_pci = auto

# Whether to disable other outputs while a virtual display is active
isolated = true
```

Wired to the WebUI in Phase 5.

## Why a multi-backend abstraction at all?

Each option has narrow but meaningful applicability:

- **KWin Virtual Output** — best Plasma path; HDR-capable; needs Plasma 6.4+ (or our plugin) for clean API.
- **Mutter `--headless --virtual-monitor`** — battle-tested for GNOME headless and remote-desktop scenarios.
- **wlroots headless** — clean, scriptable; only on Sway/Hyprland.
- **Xorg + NVIDIA** — boring but rock-solid where Wayland HDR is still maturing on NVIDIA.
- **xf86-video-dummy** — ancient but works everywhere on X11; software-only render path makes it a fallback, not a default.
- **`amdgpu.virtual_display`** — AMD's clean answer; full hardware accel through virtual CRTCs; one-time installer step.
- **EVDI** — vendor-agnostic emergency exit; we'd rather not ship a DKMS dependency, but it's there.

The factory tries the most capable / least invasive option first and degrades gracefully.

## Adding a new backend

1. Drop `backends/<my_backend>.cpp` with a private stub class implementing `IBackend`, plus a `make_<my_backend>()` factory function.
2. Declare that factory function in `backends/all.h`.
3. Add it to `kFactories` in `factory.cpp` at the right priority.
4. Add the `.cpp` and any `.h` to `cmake/compile_definitions/linux.cmake` under `PLATFORM_TARGET_FILES`.
5. Add a row to the status table above.

## Wiring point in process.cpp

Apollo's `process.cpp` has a `#ifdef _WIN32` block (around line 236) that currently calls `VDISPLAY::createVirtualDisplay()` (SudoVDA). Phase 2C adds a sibling `#elif __linux__` block that does the equivalent via this module:

```cpp
#elif __linux__
  if (config::video.headless_mode || launch_session->virtual_display || _app.virtual_display) {
    auto env = sonnenschein::vdisplay::detect();
    auto backend = sonnenschein::vdisplay::select_backend(env, vdisplay_cfg);
    if (backend) {
      sonnenschein::vdisplay::CreateRequest req{
          /*client_uid=*/device_uuid_str,
          /*client_name=*/device_name,
          /*width=*/render_width,
          /*height=*/render_height,
          /*refresh_mhz=*/target_fps,
          /*hdr_capable=*/launch_session->dynamic_range_mode > 0,
      };
      if (auto handle = backend->create(req)) {
        launch_session->virtual_display = true;
        this->virtual_display = true;
        this->display_name = handle->output_name;
        config::video.output_name = display_device::map_display_name(handle->output_name);
        // Track for cleanup on session end.
      }
    }
  }
#endif
```

Phase 2C will land this with proper lifecycle (RAII handle for cleanup, integration with `display_device::revert_configuration`).
