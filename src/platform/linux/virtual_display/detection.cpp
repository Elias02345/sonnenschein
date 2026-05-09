/**
 * @file src/platform/linux/virtual_display/detection.cpp
 * @brief Implementation of runtime environment detection.
 */
#include "detection.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace sonnenschein::vdisplay {
  namespace fs = std::filesystem;

  namespace {

    /// Read whole file or return empty on any error.
    std::string read_file(const fs::path& p) noexcept {
      std::ifstream f(p);
      if (!f) {
        return {};
      }
      std::stringstream ss;
      ss << f.rdbuf();
      return ss.str();
    }

    /// Strip trailing whitespace and newlines.
    std::string trim_right(std::string s) noexcept {
      while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
      }
      return s;
    }

    /// Read environment variable, returning empty string if unset.
    std::string env(const char* name) noexcept {
      const char* v = std::getenv(name);
      return v ? std::string(v) : std::string();
    }

    /// Run a command, capture stdout (first 8 KB), discard stderr.
    /// Returns empty string on spawn failure or non-zero exit.
    /// Uses popen — fine for our short, trusted commands.
    std::string capture(const std::string& cmd) noexcept {
      std::string redirected = cmd + " 2>/dev/null";
      std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(redirected.c_str(), "r"), pclose);
      if (!pipe) {
        return {};
      }
      std::array<char, 1024> buf{};
      std::string out;
      out.reserve(2048);
      while (auto n = std::fread(buf.data(), 1, buf.size(), pipe.get())) {
        out.append(buf.data(), n);
        if (out.size() >= 8192) {
          break;
        }
      }
      return out;
    }

    /// Detect display server from environment.
    DisplayServer detect_display_server() noexcept {
      if (!env("WAYLAND_DISPLAY").empty()) {
        return DisplayServer::Wayland;
      }
      if (!env("DISPLAY").empty()) {
        return DisplayServer::Xorg;
      }
      // Fallback: XDG_SESSION_TYPE
      const auto sess = env("XDG_SESSION_TYPE");
      if (sess == "wayland") {
        return DisplayServer::Wayland;
      }
      if (sess == "x11") {
        return DisplayServer::Xorg;
      }
      return DisplayServer::None;
    }

    /// Detect compositor from XDG_CURRENT_DESKTOP / XDG_SESSION_DESKTOP plus
    /// running-process heuristics. Display-server-aware so we distinguish
    /// KDE Wayland from KDE X11 etc.
    Compositor detect_compositor(DisplayServer ds) noexcept {
      const auto desktop_raw = env("XDG_CURRENT_DESKTOP");
      const auto session_raw = env("XDG_SESSION_DESKTOP");

      // Lowercase for matching.
      auto lower = [](std::string s) {
        for (auto& c : s) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
      };
      const auto desktop = lower(desktop_raw);
      const auto session = lower(session_raw);
      const auto either = desktop + ":" + session;

      auto contains = [&](const char* needle) {
        return either.find(needle) != std::string::npos;
      };

      const bool is_wl = (ds == DisplayServer::Wayland);

      if (contains("kde") || contains("plasma")) {
        return is_wl ? Compositor::KwinWayland : Compositor::KwinX11;
      }
      if (contains("gnome")) {
        return is_wl ? Compositor::Mutter : Compositor::MutterX11;
      }
      if (contains("sway")) {
        return Compositor::Sway;
      }
      if (contains("hyprland")) {
        return Compositor::Hyprland;
      }
      if (contains("cosmic")) {
        return Compositor::Cosmic;
      }
      if (contains("niri")) {
        return Compositor::Niri;
      }

      // Last-resort: peek at common Wayland compositor sockets / process names.
      // This catches headless boots where XDG vars aren't set.
      if (is_wl) {
        if (!capture("pgrep -x kwin_wayland").empty()) {
          return Compositor::KwinWayland;
        }
        if (!capture("pgrep -x sway").empty()) {
          return Compositor::Sway;
        }
        if (!capture("pgrep -x Hyprland").empty()) {
          return Compositor::Hyprland;
        }
        if (!capture("pgrep -x gnome-shell").empty()) {
          return Compositor::Mutter;
        }
      }

      if (ds == DisplayServer::Xorg) {
        return Compositor::Xorg;
      }
      return Compositor::Unknown;
    }

    /// Best-effort: get version string of the running compositor.
    std::string detect_compositor_version(Compositor c) noexcept {
      switch (c) {
        case Compositor::KwinWayland:
        case Compositor::KwinX11: {
          // Plasma 6 ships `kwin_wayland --version` and `kwin_x11 --version`.
          auto out = capture("kwin_wayland --version");
          if (out.empty()) {
            out = capture("kwin_x11 --version");
          }
          // Output looks like "kwin_wayland 6.4.2"
          std::regex re(R"((\d+\.\d+(?:\.\d+)?))");
          std::smatch m;
          if (std::regex_search(out, m, re)) {
            return m[1].str();
          }
          return {};
        }
        case Compositor::Mutter:
        case Compositor::MutterX11: {
          auto out = capture("gnome-shell --version");
          std::regex re(R"((\d+\.\d+(?:\.\d+)?))");
          std::smatch m;
          if (std::regex_search(out, m, re)) {
            return m[1].str();
          }
          return {};
        }
        case Compositor::Sway: {
          auto out = capture("sway --version");
          std::regex re(R"((\d+\.\d+(?:\.\d+)?))");
          std::smatch m;
          if (std::regex_search(out, m, re)) {
            return m[1].str();
          }
          return {};
        }
        case Compositor::Hyprland: {
          auto out = capture("Hyprland --version");
          std::regex re(R"(v?(\d+\.\d+(?:\.\d+)?))");
          std::smatch m;
          if (std::regex_search(out, m, re)) {
            return m[1].str();
          }
          return {};
        }
        default:
          return {};
      }
    }

    GpuVendor parse_pci_vendor(const std::string& vid_hex) noexcept {
      // Common PCI vendor IDs.
      if (vid_hex == "0x10de" || vid_hex == "10de") {
        return GpuVendor::Nvidia;
      }
      if (vid_hex == "0x1002" || vid_hex == "1002" ||  // AMD/ATI
          vid_hex == "0x1022" || vid_hex == "1022") {  // AMD (CPU vendor)
        return GpuVendor::Amd;
      }
      if (vid_hex == "0x8086" || vid_hex == "8086") {
        return GpuVendor::Intel;
      }
      return GpuVendor::Other;
    }

    /// Read a text-attribute file in /sys (e.g. .../device/vendor) and return
    /// trimmed contents.
    std::string sysfs_text(const fs::path& p) noexcept {
      return trim_right(read_file(p));
    }

    /// Resolve a /sys/class/drm/cardN -> PCI bus address by walking
    /// the device symlink. /sys/class/drm/card0/device -> ../../../0000:01:00.0
    std::string drm_card_to_pci(const fs::path& card_dir) noexcept {
      std::error_code ec;
      auto target = fs::read_symlink(card_dir / "device", ec);
      if (ec) {
        return {};
      }
      // Get the last path component
      const auto last = target.filename().string();
      // PCI ids look like 0000:01:00.0
      if (last.find(':') != std::string::npos && last.find('.') != std::string::npos) {
        return last;
      }
      return {};
    }

    /// Translate a PCI address like "0000:01:00.0" to a model string via
    /// `lspci -mm -s 0000:01:00.0`. Format: '01:00.0 "VGA compatible controller" "NVIDIA..." "..."'.
    std::string lspci_model(const std::string& pci_id) noexcept {
      // Strip leading "0000:" if present (lspci wants "01:00.0" by default)
      std::string short_id = pci_id;
      if (short_id.size() > 5 && short_id.substr(4, 1) == ":") {
        short_id = short_id.substr(5);
      }
      auto out = capture("lspci -mm -s " + short_id);
      // Parse: 01:00.0 "Class" "Vendor" "Device" "..." "..."
      std::regex re(R"(\"[^\"]*\"\s+\"([^\"]+)\"\s+\"([^\"]+)\")");
      std::smatch m;
      if (std::regex_search(out, m, re)) {
        return m[1].str() + " " + m[2].str();
      }
      return {};
    }

    std::string detect_nvidia_driver_version() noexcept {
      // /proc/driver/nvidia/version is the canonical source when the
      // proprietary or open kernel module is loaded.
      auto raw = read_file("/proc/driver/nvidia/version");
      if (!raw.empty()) {
        std::regex re(R"(Module\s+(\d+\.\d+\.\d+))");
        std::smatch m;
        if (std::regex_search(raw, m, re)) {
          return m[1].str();
        }
      }
      // Fallback to nvidia-smi (slower).
      auto out = capture("nvidia-smi --query-gpu=driver_version --format=csv,noheader,nounits");
      return trim_right(std::move(out));
    }

    std::string detect_mesa_version() noexcept {
      // Cheap: glxinfo isn't always installed; try modinfo on the driver.
      // Not strictly per-GPU but a useful diagnostic.
      auto out = capture("pkg-config --modversion mesa-dri-drivers");
      if (!out.empty()) {
        return trim_right(std::move(out));
      }
      // glxinfo is a heavier fallback.
      out = capture("glxinfo -B 2>/dev/null | grep -i 'OpenGL version' | head -1");
      std::regex re(R"((\d+\.\d+(?:\.\d+)?))");
      std::smatch m;
      if (std::regex_search(out, m, re)) {
        return m[1].str();
      }
      return {};
    }

    /// Walk /sys/class/drm/card* to enumerate render-capable GPUs. Skip
    /// virtual outputs and EVDI cards (we'll create those ourselves later).
    std::vector<GpuInfo> enumerate_gpus() noexcept {
      std::vector<GpuInfo> out;
      std::error_code ec;
      const fs::path drm_root = "/sys/class/drm";
      if (!fs::exists(drm_root, ec)) {
        return out;
      }

      // We want one entry per physical GPU. Cards (cardN) and renderD nodes
      // both point at the same device, so dedupe by PCI address.
      std::vector<std::string> seen_pci;

      for (const auto& entry : fs::directory_iterator(drm_root, ec)) {
        if (ec) {
          break;
        }
        const auto name = entry.path().filename().string();
        if (name.rfind("card", 0) != 0) {
          continue;
        }
        // Skip card output nodes like "card0-DP-1"
        if (name.find('-') != std::string::npos) {
          continue;
        }

        const auto pci_id = drm_card_to_pci(entry.path());
        if (pci_id.empty()) {
          continue;
        }
        if (std::find(seen_pci.begin(), seen_pci.end(), pci_id) != seen_pci.end()) {
          continue;
        }
        seen_pci.push_back(pci_id);

        const fs::path device_dir = entry.path() / "device";
        const auto vendor_hex = sysfs_text(device_dir / "vendor");
        const auto vendor = parse_pci_vendor(vendor_hex);

        GpuInfo gpu;
        gpu.pci_id = pci_id;
        gpu.vendor = vendor;
        gpu.model = lspci_model(pci_id);
        gpu.drm_node = "/dev/dri/" + name;

        // Driver version per vendor.
        switch (vendor) {
          case GpuVendor::Nvidia:
            gpu.driver_version = detect_nvidia_driver_version();
            gpu.has_nvenc = !gpu.driver_version.empty();
            break;
          case GpuVendor::Amd:
          case GpuVendor::Intel:
            gpu.driver_version = detect_mesa_version();
            gpu.has_vaapi = true;  // best-effort; encoder probe is authoritative
            break;
          default:
            break;
        }

        out.push_back(std::move(gpu));
      }
      return out;
    }

    /// Pick the GPU we'd default to. Preference: NVIDIA (best NVENC) > AMD > Intel.
    /// Multi-GPU laptops with hybrid graphics: this picks the dGPU.
    int choose_primary(const std::vector<GpuInfo>& gpus) noexcept {
      auto rank = [](GpuVendor v) {
        switch (v) {
          case GpuVendor::Nvidia: return 3;
          case GpuVendor::Amd:    return 2;
          case GpuVendor::Intel:  return 1;
          default: return 0;
        }
      };
      int best = -1;
      int best_rank = -1;
      for (size_t i = 0; i < gpus.size(); ++i) {
        const auto r = rank(gpus[i].vendor);
        if (r > best_rank) {
          best = static_cast<int>(i);
          best_rank = r;
        }
      }
      return best;
    }

  }  // namespace

  Environment detect() {
    Environment env_out;
    env_out.display_server = detect_display_server();
    env_out.compositor = detect_compositor(env_out.display_server);
    env_out.compositor_version = detect_compositor_version(env_out.compositor);
    env_out.gpus = enumerate_gpus();
    env_out.primary_gpu_idx = choose_primary(env_out.gpus);
    return env_out;
  }

  std::string to_string(DisplayServer s) {
    switch (s) {
      case DisplayServer::Wayland: return "Wayland";
      case DisplayServer::Xorg:    return "Xorg";
      case DisplayServer::None:    return "None";
    }
    return "Unknown";
  }

  std::string to_string(Compositor c) {
    switch (c) {
      case Compositor::KwinWayland: return "KWin (Wayland)";
      case Compositor::KwinX11:     return "KWin (X11)";
      case Compositor::Mutter:      return "Mutter (Wayland)";
      case Compositor::MutterX11:   return "Mutter (X11)";
      case Compositor::Sway:        return "Sway";
      case Compositor::Hyprland:    return "Hyprland";
      case Compositor::Cosmic:      return "Cosmic";
      case Compositor::Niri:        return "Niri";
      case Compositor::Xorg:        return "Xorg (no compositor detected)";
      case Compositor::Unknown:     return "Unknown";
    }
    return "Unknown";
  }

  std::string to_string(GpuVendor v) {
    switch (v) {
      case GpuVendor::Nvidia:  return "NVIDIA";
      case GpuVendor::Amd:     return "AMD";
      case GpuVendor::Intel:   return "Intel";
      case GpuVendor::Other:   return "Other";
      case GpuVendor::Unknown: return "Unknown";
    }
    return "Unknown";
  }

}  // namespace sonnenschein::vdisplay
