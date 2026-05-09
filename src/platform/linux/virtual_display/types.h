/**
 * @file src/platform/linux/virtual_display/types.h
 * @brief Sonnenschein virtual-display abstraction — common types.
 *
 * Sonnenschein creates a virtual display matching the Moonlight client's
 * resolution, refresh rate, and HDR capability when pairing happens. On
 * Linux, the right path differs by GPU vendor and compositor; this module
 * abstracts that so process.cpp can stay thin.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sonnenschein::vdisplay {

  enum class DisplayServer : uint8_t {
    Wayland,
    Xorg,
    None,  ///< headless boot, neither WAYLAND_DISPLAY nor DISPLAY set
  };

  /// Detected compositor. The X11 variants share Xorg display server but
  /// behave differently w.r.t. virtual-display creation.
  enum class Compositor : uint8_t {
    KwinWayland,   ///< KDE Plasma 6+ Wayland — primary Sonnenschein target
    KwinX11,       ///< KDE Plasma X11 session
    Mutter,        ///< GNOME Wayland
    MutterX11,     ///< GNOME X11 session
    Sway,          ///< wlroots reference compositor
    Hyprland,      ///< wlroots fork
    Cosmic,        ///< System76 Cosmic (also wlroots-derived)
    Niri,          ///< niri (smithay)
    Xorg,          ///< plain Xorg, unknown WM (e.g. SteamOS Big Picture)
    Unknown,
  };

  enum class GpuVendor : uint8_t {
    Nvidia,
    Amd,
    Intel,
    Other,    ///< e.g. virtual GPU, ASPEED, Matrox — not for streaming
    Unknown,
  };

  struct GpuInfo {
    /// PCI bus identifier, e.g. "0000:01:00.0". Used as stable handle for
    /// "pin streaming to this GPU".
    std::string pci_id;

    GpuVendor vendor = GpuVendor::Unknown;

    /// Human-readable model, e.g. "NVIDIA GeForce RTX 3070".
    std::string model;

    /// Driver version string. NVIDIA: "595.71.05". Mesa-side: "25.2.8".
    std::string driver_version;

    /// /dev/dri/renderD12N or /dev/dri/cardN node corresponding to this GPU.
    /// Empty if no DRM node found (e.g. NVIDIA proprietary not yet using KMS).
    std::string drm_node;

    /// Heuristic capabilities — informational only, encoder probe still runs.
    bool has_nvenc = false;   ///< NVIDIA only: NVENC hardware encoder usable
    bool has_vaapi = false;   ///< has a VAAPI-capable encoder (AMD/Intel typically)
  };

  struct Environment {
    DisplayServer display_server = DisplayServer::None;
    Compositor compositor = Compositor::Unknown;

    /// Best-effort version string (e.g. "6.4.2" for KWin, "47.3" for Mutter).
    /// Empty when we can't determine it.
    std::string compositor_version;

    /// All GPUs Sonnenschein can possibly stream from. May be empty inside
    /// containers without /dev/dri.
    std::vector<GpuInfo> gpus;

    /// Index into `gpus` for the GPU we'd default to. Negative if no GPU
    /// is suitable for streaming.
    int primary_gpu_idx = -1;

    /// Convenience: is at least one Wayland-capable compositor running?
    [[nodiscard]] bool is_wayland() const noexcept {
      return display_server == DisplayServer::Wayland;
    }
  };

  /// What the streaming server tells us about a newly-paired client. Maps
  /// directly to the RTSP SETUP / launch_session fields Apollo already
  /// parses; we just bundle them.
  struct CreateRequest {
    /// Stable per-client UUID from launch_session->unique_id (or app+client
    /// XOR per Apollo's per_client_app_identity logic). Used so the same
    /// client always gets a display with the same identity → its display
    /// configuration is remembered across reconnects.
    std::string client_uid;

    /// Human-readable client name, e.g. "elias's Steam Deck OLED".
    std::string client_name;

    uint32_t width = 0;
    uint32_t height = 0;

    /// Refresh rate in milli-Hertz (Apollo's convention), e.g. 90000 for 90 Hz.
    uint32_t refresh_mhz = 0;

    /// Client signaled it can decode a 10-bit HDR stream.
    bool hdr_capable = false;
  };

  /// Live handle to a virtual display. The backend keeps internal state
  /// keyed by display_id; the rest of the codebase only sees this struct.
  struct Handle {
    /// Backend-internal identifier (e.g. KWin output name "Virtual-1",
    /// Xorg DEVMODE.DeviceName, EVDI card index).
    std::string display_id;

    /// Name as it appears in the display-device list — what
    /// display_device::map_display_name() would resolve to.
    /// Used to set config::video.output_name.
    std::string output_name;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refresh_mhz = 0;
    bool hdr_active = false;

    /// PCI id of the GPU the display was tied to (for multi-GPU).
    std::string gpu_pci_id;
  };

}  // namespace sonnenschein::vdisplay
