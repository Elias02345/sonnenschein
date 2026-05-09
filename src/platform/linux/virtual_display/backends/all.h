/**
 * @file src/platform/linux/virtual_display/backends/all.h
 * @brief Factory functions for every IBackend implementation Sonnenschein
 * ships. Each lives in its own .cpp with a private stub class.
 *
 * Phase 2A: all returns return stubs whose `available()` is mostly false
 * and whose `create()` always returns nullopt. The stubs exist so the
 * factory selection logic can be written and tested before the real
 * backends land in Phase 2B+.
 */
#pragma once

#include "../interface.h"

#include <memory>

namespace sonnenschein::vdisplay::backends {

  /// KDE Plasma 6 Wayland — primary Sonnenschein target. Phase 2B target.
  /// Uses kscreen-doctor `add-virtual-output` (Plasma 6.4+) or KWin DBus,
  /// with a custom KWin plugin as last-resort.
  std::unique_ptr<IBackend> make_kwin_wayland();

  /// GNOME / Mutter Wayland headless. `mutter --headless --virtual-monitor`
  /// is mature; we drive it via the org.gnome.Mutter.RemoteDesktop and
  /// org.gnome.Mutter.ScreenCast D-Bus interfaces.
  std::unique_ptr<IBackend> make_mutter_headless();

  /// Sway and reasonably-current Hyprland. WLR_BACKENDS=headless plus
  /// `swaymsg create_output` / hyprctl equivalents.
  std::unique_ptr<IBackend> make_wlroots_headless();

  /// Xorg + NVIDIA proprietary driver. Generates a custom xorg.conf with
  /// `--virtual=WxH` plus an EDID file matching the client mode, then
  /// applies it via xrandr. Works on any NVIDIA GPU with proprietary or
  /// open kernel module.
  std::unique_ptr<IBackend> make_xorg_nvidia();

  /// Xorg + AMD/Intel via xf86-video-dummy. Uses CVT-reduced-blanking
  /// modelines and xrandr --newmode for arbitrary resolutions.
  std::unique_ptr<IBackend> make_xorg_dummy();

  /// AMD-only. Sets the `amdgpu.virtual_display=PCI:..., N` kernel parameter
  /// (one-time installer step) which exposes virtual CRTCs the GPU can
  /// natively render to.
  std::unique_ptr<IBackend> make_amdgpu_param();

  /// Last-resort, vendor-agnostic. Loads the EVDI DKMS kernel module and
  /// creates a virtual display device. Works everywhere but adds a kernel
  /// dependency we'd rather avoid as a default.
  std::unique_ptr<IBackend> make_evdi();

}  // namespace sonnenschein::vdisplay::backends
