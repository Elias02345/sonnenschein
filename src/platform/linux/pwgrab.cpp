/**
 * @file src/platform/linux/pwgrab.cpp
 * @brief PipeWire portal-based screen capture for virtual displays.
 *
 * Uses xdg-desktop-portal ScreenCast D-Bus API to obtain a PipeWire
 * stream node, then connects via libpipewire to receive DMA-BUF or
 * SHM frames. This is the only capture method that can see KWin
 * virtual outputs (which have no DRM connector).
 */
#ifdef SUNSHINE_BUILD_PIPEWIRE

// standard includes
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <limits.h>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <poll.h>
#include <pwd.h>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

// pipewire includes
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/debug/types.h>
#include <spa/param/video/raw.h>
#include <spa/utils/result.h>

// GLib / GDBus for portal communication
#include <gio/gio.h>

#ifdef SUNSHINE_BUILD_KWIN
#include <kde-output-device-v2.h>
#include <kde-output-management-v2.h>
#include <zkde-screencast-unstable-v1.h>
#include "virtual_display/subprocess.h"
#endif

// local includes
#include "src/logging.h"
#include "src/platform/common.h"
#include "src/video.h"
#include "vaapi.h"
#include "graphics.h"

using namespace std::literals;

namespace pw {

  // Portal D-Bus constants
  static constexpr auto PORTAL_BUS_NAME = "org.freedesktop.portal.Desktop";
  static constexpr auto PORTAL_OBJECT   = "/org/freedesktop/portal/desktop";
  static constexpr auto PORTAL_SCREENCAST = "org.freedesktop.portal.ScreenCast";
  static constexpr auto PORTAL_REQUEST  = "org.freedesktop.portal.Request";
  static constexpr uint32_t SOURCE_MONITOR = 1;
  static constexpr uint32_t SOURCE_WINDOW = 2;
  static constexpr uint32_t SOURCE_VIRTUAL = 4;
  static constexpr uint32_t CURSOR_HIDDEN = 1;
  static constexpr uint32_t CURSOR_EMBEDDED = 2;
  static constexpr uint32_t CURSOR_METADATA = 4;

  // Helper: unique request token
  static std::atomic<uint32_t> request_counter{0};

  static std::string make_token() {
    return "sonnenschein_" + std::to_string(++request_counter);
  }

  static std::string make_request_path(GDBusConnection *conn, const std::string &token) {
    auto name = g_dbus_connection_get_unique_name(conn);
    std::string sender(name ? name : "");
    // Replace dots and leading colon
    for (auto &c : sender) {
      if (c == '.' || c == ':') c = '_';
    }
    if (!sender.empty() && sender[0] == '_') sender = sender.substr(1);
    return "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
  }

  static uint32_t normalize_framerate_hz(uint32_t requested_framerate) {
    if (requested_framerate == 0) {
      return 60;
    }
    if (requested_framerate > 1000) {
      return std::max<uint32_t>(1, (requested_framerate + 500) / 1000);
    }
    return requested_framerate;
  }

#ifdef SUNSHINE_BUILD_KWIN
  /**
   * Direct KWin ScreenCast session.
   *
   * This bypasses xdg-desktop-portal and requests a PipeWire node directly
   * for the wl_output named by the virtual-display backend. KDE's portal
   * "Virtual Display" source is a separate XDG virtual monitor and has been
   * observed to force 1920x1080, so we must target the existing output.
   */
  struct kwin_session_t {
    struct output_t {
      wl_output *output = nullptr;
      std::string name;
      std::string description;
      int width = 0;
      int height = 0;
      int x = 0;
      int y = 0;
    };

    struct kde_mode_t {
      kde_output_device_mode_v2 *mode = nullptr;
      int width = 0;
      int height = 0;
      int32_t refresh_mhz = 0;
      uint32_t flags = 0;
      bool preferred = false;
      bool removed = false;
    };

    struct kde_output_t {
      kde_output_device_v2 *device = nullptr;
      std::string name;
      std::string make;
      std::string model;
      int x = 0;
      int y = 0;
      bool enabled = false;
      bool removed = false;
      uint32_t capabilities = 0;
      bool hdr_enabled = false;
      bool wcg_enabled = false;
      kde_output_device_mode_v2 *current_mode = nullptr;
      std::vector<std::unique_ptr<kde_mode_t>> modes;
    };

    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    zkde_screencast_unstable_v1 *screencast = nullptr;
    zkde_screencast_stream_unstable_v1 *stream = nullptr;
    kde_output_management_v2 *output_management = nullptr;
    kde_output_device_registry_v2 *output_device_registry = nullptr;
    std::map<wl_output *, output_t> outputs;
    std::map<kde_output_device_v2 *, std::unique_ptr<kde_output_t>> kde_outputs;
    std::map<kde_output_device_mode_v2 *, kde_mode_t *> kde_modes;
    uint32_t output_management_version = 0;
    uint32_t node_id = PW_ID_ANY;
    int width = 0;
    int height = 0;
    bool ready = false;
    bool failed = false;
    bool hdr_requested = false;
    bool hdr_configured = false;
    bool hdr_confirmed = false;
    bool mode_configured = false;
    std::string error;

    ~kwin_session_t() {
      if (stream) {
        zkde_screencast_stream_unstable_v1_close(stream);
        stream = nullptr;
      }
      if (screencast) {
        zkde_screencast_unstable_v1_destroy(screencast);
        screencast = nullptr;
      }
      for (auto &[_, output] : kde_outputs) {
        for (auto &mode : output->modes) {
          if (mode->mode && !mode->removed) {
            kde_output_device_mode_v2_destroy(mode->mode);
            mode->mode = nullptr;
          }
        }
        if (output->device && !output->removed) {
          kde_output_device_v2_release(output->device);
          output->device = nullptr;
        }
      }
      kde_modes.clear();
      kde_outputs.clear();
      if (output_device_registry) {
        kde_output_device_registry_v2_destroy(output_device_registry);
        output_device_registry = nullptr;
      }
      if (output_management) {
        kde_output_management_v2_destroy(output_management);
        output_management = nullptr;
      }
      for (auto &[output, _] : outputs) {
        wl_output_destroy(output);
      }
      outputs.clear();
      if (registry) {
        wl_registry_destroy(registry);
        registry = nullptr;
      }
      if (display) {
        wl_display_disconnect(display);
        display = nullptr;
      }
    }

    bool init() {
      setup_permission();

      const char *wl_name = std::getenv("WAYLAND_DISPLAY");
      if (!wl_name) {
        BOOST_LOG(warning) << "KWin direct capture: WAYLAND_DISPLAY is not set"sv;
        return false;
      }

      display = wl_display_connect(wl_name);
      if (!display) {
        BOOST_LOG(warning) << "KWin direct capture: couldn't connect to Wayland display "sv << wl_name;
        return false;
      }

      registry = wl_display_get_registry(display);
      wl_registry_add_listener(registry, &registry_listener, this);
      wl_display_roundtrip(display);
      wl_display_roundtrip(display);

      if (!screencast) {
        BOOST_LOG(warning)
          << "KWin direct capture: zkde_screencast_unstable_v1 not available; "
          << "falling back to xdg-desktop-portal monitor capture"sv;
        return false;
      }
      return true;
    }

    bool start(
      const std::string &target_output_name,
      int requested_width,
      int requested_height,
      double requested_framerate,
      bool requested_hdr
    ) {
      hdr_requested = requested_hdr;
      wl_output *target = nullptr;
      output_t *params = nullptr;
      
      auto find_target = [&]() -> bool {
        // 1. Try exact match by name or description
        for (auto &[output, candidate] : outputs) {
          BOOST_LOG(info) << "KWin direct capture: found output '"sv << candidate.name
                          << "' (desc: '"sv << candidate.description << "') "sv
                          << candidate.width << "x"sv << candidate.height
                          << " at "sv << candidate.x << ","sv << candidate.y;
          if (candidate.name == target_output_name || candidate.description == target_output_name) {
            target = output;
            params = &candidate;
          }
        }

        // 2. If not found, try fallback: the first output starting with 'Virtual-'
        // or description containing 'Virtual'
        if (!target) {
          for (auto &[output, candidate] : outputs) {
            if (candidate.name.find("Virtual-") == 0 || candidate.description.find("Virtual") != std::string::npos) {
              BOOST_LOG(info) << "KWin direct capture: using fallback virtual output '"sv << candidate.name << "'";
              target = output;
              params = &candidate;
              break;
            }
          }
        }
        
        return target != nullptr;
      };

      if (!find_target()) {
        BOOST_LOG(info) << "KWin direct capture: target not found yet, doing a display roundtrip to sync hotplug events...";
        wl_display_roundtrip(display);
        wl_display_roundtrip(display);
        find_target(); // Try again after syncing
      }

      if (target && params) {
        mode_configured = apply_output_management_settings(
          target_output_name,
          requested_width > 0 ? requested_width : params->width,
          requested_height > 0 ? requested_height : params->height,
          requested_framerate,
          requested_hdr,
          2500ms);
        stream = zkde_screencast_unstable_v1_stream_output(
          screencast,
          target,
          ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_EMBEDDED);
        width = params->width;
        height = params->height;
      } else {
        BOOST_LOG(info) << "KWin direct capture: output '"sv << target_output_name
                        << "' not found in Wayland registry. Requesting KWin to create a stream_virtual_output directly.";
        stream = zkde_screencast_unstable_v1_stream_virtual_output(
          screencast,
          target_output_name.c_str(),
          requested_width > 0 ? requested_width : 1920,
          requested_height > 0 ? requested_height : 1080,
          wl_fixed_from_double(1.0),
          ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_EMBEDDED);
        width = requested_width > 0 ? requested_width : 1920;
        height = requested_height > 0 ? requested_height : 1080;
      }

      if (!stream) {
        BOOST_LOG(warning) << "KWin direct capture: stream_output or stream_virtual_output returned null"sv;
        return false;
      }
      zkde_screencast_stream_unstable_v1_add_listener(stream, &stream_listener, this);

      if (!mode_configured) {
        mode_configured = apply_output_management_settings(
          target_output_name,
          width,
          height,
          requested_framerate,
          requested_hdr,
          2500ms);
      }

      if (!wait_for_stream()) {
        BOOST_LOG(warning) << "KWin direct capture: "sv << (error.empty() ? "timeout waiting for stream" : error);
        return false;
      }

      if (!mode_configured) {
        mode_configured = apply_output_management_settings(
          target_output_name,
          width,
          height,
          requested_framerate,
          requested_hdr,
          1000ms);
      }
      if (!mode_configured) {
        apply_refresh_rate(target_output_name, width, height, requested_framerate);
      }

      BOOST_LOG(info) << "KWin direct capture: streaming output '"
                      << target_output_name << "' " << width << "x" << height
                      << " node_id=" << node_id;
      return node_id != PW_ID_ANY && width > 0 && height > 0;
    }

    bool is_hdr_active() const {
      return hdr_requested;
    }

    bool is_hdr_confirmed() const {
      return hdr_confirmed;
    }

  private:
    struct mode_t {
      int width = 0;
      int height = 0;
      double refresh_hz = 0.0;
    };

    struct kscreen_output_t {
      std::string name;
      std::string raw;
      bool enabled = false;
      bool connected = false;
      bool current_mode_confirmed = false;
      mode_t current_mode;
    };

    struct kscreen_snapshot_t {
      bool ok = false;
      std::string raw;
      std::vector<kscreen_output_t> outputs;
    };

    static std::string trim_copy(const std::string &value) {
      auto begin = value.begin();
      while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
      }

      auto end = value.end();
      while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
      }

      return {begin, end};
    }

    static bool starts_with(const std::string &value, const std::string &prefix) {
      return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }

    static bool contains_case_insensitive(std::string value, std::string needle) {
      std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
      std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
      return value.find(needle) != std::string::npos;
    }

    static bool has_mode(const mode_t &mode) {
      return mode.width > 0 && mode.height > 0 && mode.refresh_hz > 0.0;
    }

    static std::string format_refresh_hz(double hz) {
      const auto rounded = std::round(hz);
      if (std::fabs(hz - rounded) < 0.001) {
        return std::to_string(static_cast<int>(rounded));
      }

      std::ostringstream out;
      out << std::fixed << std::setprecision(3) << hz;
      auto text = out.str();
      while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
      }
      if (!text.empty() && text.back() == '.') {
        text.pop_back();
      }
      return text;
    }

    static double normalize_refresh_hz(double requested_framerate) {
      if (requested_framerate <= 0.0) {
        return 0.0;
      }
      if (requested_framerate > 1000.0) {
        return requested_framerate / 1000.0;
      }
      return requested_framerate;
    }

    static std::string format_mode(int width, int height, double refresh_hz) {
      const char at_sign = 0x40;
      std::ostringstream out;
      out << width << "x" << height << at_sign << format_refresh_hz(refresh_hz);
      return out.str();
    }

    static std::optional<mode_t> parse_mode_from_text(const std::string &line) {
      for (size_t at_pos = line.find(static_cast<char>(0x40));
           at_pos != std::string::npos;
           at_pos = line.find(static_cast<char>(0x40), at_pos + 1)) {
        const auto x_pos = line.rfind('x', at_pos);
        if (x_pos == std::string::npos || x_pos == 0 || x_pos + 1 >= at_pos) {
          continue;
        }

        size_t width_begin = x_pos;
        while (width_begin > 0 && std::isdigit(static_cast<unsigned char>(line[width_begin - 1]))) {
          --width_begin;
        }
        if (width_begin == x_pos) {
          continue;
        }

        bool height_digits = true;
        for (size_t i = x_pos + 1; i < at_pos; ++i) {
          if (!std::isdigit(static_cast<unsigned char>(line[i]))) {
            height_digits = false;
            break;
          }
        }
        if (!height_digits) {
          continue;
        }

        size_t refresh_end = at_pos + 1;
        while (
          refresh_end < line.size() &&
          (std::isdigit(static_cast<unsigned char>(line[refresh_end])) || line[refresh_end] == '.')
        ) {
          ++refresh_end;
        }
        if (refresh_end == at_pos + 1) {
          continue;
        }

        mode_t mode;
        try {
          mode.width = std::stoi(line.substr(width_begin, x_pos - width_begin));
          mode.height = std::stoi(line.substr(x_pos + 1, at_pos - x_pos - 1));
          mode.refresh_hz = std::stod(line.substr(at_pos + 1, refresh_end - at_pos - 1));
        } catch (...) {
          continue;
        }

        if (has_mode(mode)) {
          return mode;
        }
      }

      return std::nullopt;
    }

    static std::optional<std::pair<int, int>> parse_geometry_from_text(const std::string &line) {
      if (line.find("Geometry:") == std::string::npos) {
        return std::nullopt;
      }

      for (size_t x_pos = line.find('x'); x_pos != std::string::npos; x_pos = line.find('x', x_pos + 1)) {
        if (x_pos == 0 || x_pos + 1 >= line.size()) {
          continue;
        }

        size_t width_begin = x_pos;
        while (width_begin > 0 && std::isdigit(static_cast<unsigned char>(line[width_begin - 1]))) {
          --width_begin;
        }
        size_t height_end = x_pos + 1;
        while (height_end < line.size() && std::isdigit(static_cast<unsigned char>(line[height_end]))) {
          ++height_end;
        }
        if (width_begin == x_pos || height_end == x_pos + 1) {
          continue;
        }

        try {
          return std::make_pair(
            std::stoi(line.substr(width_begin, x_pos - width_begin)),
            std::stoi(line.substr(x_pos + 1, height_end - x_pos - 1)));
        } catch (...) {
          return std::nullopt;
        }
      }

      return std::nullopt;
    }

    static kscreen_snapshot_t read_kscreen_outputs() {
      auto result = sonnenschein::vdisplay::run_args({"kscreen-doctor", "-o"}, "kscreen-doctor -o");

      kscreen_snapshot_t snapshot;
      snapshot.ok = result.ok;
      snapshot.raw = result.output;
      if (!result.ok) {
        return snapshot;
      }

      std::istringstream stream(result.output);
      std::string line;
      kscreen_output_t *current = nullptr;

      while (std::getline(stream, line)) {
        const auto trimmed = trim_copy(line);
        if (starts_with(trimmed, "Output: ")) {
          std::istringstream parts(trimmed);
          std::string label;
          std::string index;

          kscreen_output_t output;
          output.raw = line;
          if (parts >> label >> index >> output.name) {
            std::string token;
            while (parts >> token) {
              if (token == "enabled") {
                output.enabled = true;
              } else if (token == "connected") {
                output.connected = true;
              }
            }
            snapshot.outputs.push_back(std::move(output));
            current = &snapshot.outputs.back();
          } else {
            current = nullptr;
          }
          continue;
        }

        if (!current) {
          continue;
        }

        current->raw += '\n';
        current->raw += line;

        if (auto mode = parse_mode_from_text(trimmed)) {
          const auto lower_current = contains_case_insensitive(trimmed, "current");
          const auto has_marker = trimmed.find('*') != std::string::npos;
          if (!has_mode(current->current_mode) || lower_current || has_marker) {
            current->current_mode = *mode;
            current->current_mode_confirmed = lower_current || has_marker;
          }
        } else if (auto geometry = parse_geometry_from_text(trimmed)) {
          if (current->current_mode.width == 0) {
            current->current_mode.width = geometry->first;
          }
          if (current->current_mode.height == 0) {
            current->current_mode.height = geometry->second;
          }
        }
      }

      return snapshot;
    }

    static bool is_virtual_output(const kscreen_output_t &output) {
      return starts_with(output.name, "Virtual-") ||
             starts_with(output.name, "Sonnenschein-") ||
             output.raw.find("Virtual") != std::string::npos ||
             output.raw.find("Sonnenschein") != std::string::npos;
    }

    static bool matches_resolution(const kscreen_output_t &output, int width, int height) {
      if (width <= 0 || height <= 0) {
        return true;
      }
      return output.current_mode.width == width && output.current_mode.height == height;
    }

    static bool matches_mode(const kscreen_output_t &output, int width, int height, double refresh_hz) {
      return matches_resolution(output, width, height) &&
             output.current_mode.refresh_hz > 0.0 &&
             std::fabs(output.current_mode.refresh_hz - refresh_hz) < 0.5;
    }

    static int virtual_output_index(const kscreen_output_t &output) {
      if (!starts_with(output.name, "Virtual-")) {
        return -1;
      }

      try {
        return std::stoi(output.name.substr(std::string("Virtual-").size()));
      } catch (...) {
        return -1;
      }
    }

    static std::string describe_mode(const kscreen_output_t &output) {
      if (!has_mode(output.current_mode)) {
        if (output.current_mode.width > 0 && output.current_mode.height > 0) {
          return std::to_string(output.current_mode.width) + "x" + std::to_string(output.current_mode.height);
        }
        return "unknown";
      }
      return format_mode(output.current_mode.width, output.current_mode.height, output.current_mode.refresh_hz);
    }

    static std::optional<kscreen_output_t> resolve_kscreen_output(
      const kscreen_snapshot_t &snapshot,
      const std::string &target_output_name,
      int requested_width,
      int requested_height
    ) {
      auto active = [](const kscreen_output_t &output) {
        return output.enabled && output.connected;
      };
      auto target_match = [&](const kscreen_output_t &output) {
        return output.name == target_output_name || output.raw.find(target_output_name) != std::string::npos;
      };

      for (const auto &output : snapshot.outputs) {
        if (active(output) && target_match(output) && matches_resolution(output, requested_width, requested_height)) {
          return output;
        }
      }

      for (const auto &output : snapshot.outputs) {
        if (active(output) && target_match(output)) {
          return output;
        }
      }

      std::optional<kscreen_output_t> virtual_match;
      for (const auto &output : snapshot.outputs) {
        if (!active(output) || !is_virtual_output(output) || !matches_resolution(output, requested_width, requested_height)) {
          continue;
        }

        if (!virtual_match || virtual_output_index(output) >= virtual_output_index(*virtual_match)) {
          virtual_match = output;
        }
      }
      if (virtual_match) {
        return virtual_match;
      }

      std::optional<kscreen_output_t> single_virtual;
      int virtual_count = 0;
      for (const auto &output : snapshot.outputs) {
        if (active(output) && is_virtual_output(output)) {
          single_virtual = output;
          ++virtual_count;
        }
      }
      if (virtual_count == 1) {
        return single_virtual;
      }

      return std::nullopt;
    }

    static std::optional<kscreen_output_t> resolve_kscreen_output_by_name(
      const kscreen_snapshot_t &snapshot,
      const std::string &name
    ) {
      for (const auto &output : snapshot.outputs) {
        if (output.name == name) {
          return output;
        }
      }
      return std::nullopt;
    }

    static std::optional<kscreen_output_t> wait_for_kscreen_output(
      const std::string &target_output_name,
      int requested_width,
      int requested_height,
      std::chrono::milliseconds timeout,
      std::string &last_output
    ) {
      const auto deadline = std::chrono::steady_clock::now() + timeout;

      while (true) {
        const auto snapshot = read_kscreen_outputs();
        last_output = snapshot.raw;
        if (snapshot.ok) {
          if (auto output = resolve_kscreen_output(snapshot, target_output_name, requested_width, requested_height)) {
            return output;
          }
        }

        if (std::chrono::steady_clock::now() >= deadline) {
          break;
        }
        std::this_thread::sleep_for(150ms);
      }

      return std::nullopt;
    }

    static std::optional<kscreen_output_t> wait_for_verified_mode(
      const std::string &kscreen_name,
      int requested_width,
      int requested_height,
      double requested_hz,
      std::chrono::milliseconds timeout,
      std::string &last_output
    ) {
      const auto deadline = std::chrono::steady_clock::now() + timeout;
      std::optional<kscreen_output_t> last_seen;

      while (true) {
        const auto snapshot = read_kscreen_outputs();
        last_output = snapshot.raw;
        if (snapshot.ok) {
          last_seen = resolve_kscreen_output_by_name(snapshot, kscreen_name);
          if (last_seen && matches_mode(*last_seen, requested_width, requested_height, requested_hz)) {
            return last_seen;
          }
        }

        if (std::chrono::steady_clock::now() >= deadline) {
          break;
        }
        std::this_thread::sleep_for(150ms);
      }

      return last_seen;
    }

    static uint32_t refresh_millihz(double requested_framerate) {
      const auto hz = normalize_refresh_hz(requested_framerate);
      if (hz <= 0.0) {
        return 0;
      }
      return static_cast<uint32_t>(std::max<double>(1.0, std::llround(hz * 1000.0)));
    }

    static double mode_refresh_hz(const kde_mode_t &mode) {
      return static_cast<double>(mode.refresh_mhz) / 1000.0;
    }

    static bool has_kde_mode(const kde_mode_t &mode) {
      return !mode.removed && mode.width > 0 && mode.height > 0 && mode.refresh_mhz > 0;
    }

    static bool matches_kde_mode(const kde_mode_t &mode, int requested_width, int requested_height, uint32_t requested_mhz) {
      return has_kde_mode(mode) &&
             mode.width == requested_width &&
             mode.height == requested_height &&
             std::abs(static_cast<int64_t>(mode.refresh_mhz) - static_cast<int64_t>(requested_mhz)) <= 500;
    }

    kde_mode_t *mode_for(kde_output_device_mode_v2 *mode) const {
      const auto it = kde_modes.find(mode);
      return it == kde_modes.end() ? nullptr : it->second;
    }

    kde_mode_t *current_mode_for(const kde_output_t &output) const {
      return output.current_mode ? mode_for(output.current_mode) : nullptr;
    }

    static std::string describe_kde_mode(const kde_mode_t *mode) {
      if (!mode || !has_kde_mode(*mode)) {
        return "unknown";
      }
      return format_mode(mode->width, mode->height, mode_refresh_hz(*mode));
    }

    static std::string kde_output_label(const kde_output_t &output) {
      if (!output.name.empty()) {
        return output.name;
      }
      if (!output.model.empty()) {
        return output.model;
      }
      return "unnamed";
    }

    static bool is_kde_virtual_output(const kde_output_t &output) {
      const auto label = kde_output_label(output);
      return starts_with(label, "Virtual-") ||
             starts_with(label, "Sonnenschein-") ||
             contains_case_insensitive(label, "virtual") ||
             contains_case_insensitive(output.model, "virtual") ||
             contains_case_insensitive(output.make, "virtual") ||
             contains_case_insensitive(output.model, "sonnenschein");
    }

    bool kde_output_matches_resolution(const kde_output_t &output, int requested_width, int requested_height) const {
      if (requested_width <= 0 || requested_height <= 0) {
        return true;
      }
      if (auto *mode = current_mode_for(output); mode && mode->width > 0 && mode->height > 0) {
        return mode->width == requested_width && mode->height == requested_height;
      }
      for (const auto &mode : output.modes) {
        if (!mode->removed && mode->width == requested_width && mode->height == requested_height) {
          return true;
        }
      }
      return false;
    }

    kde_mode_t *find_kde_mode(kde_output_t &output, int requested_width, int requested_height, uint32_t requested_mhz) const {
      for (auto &mode : output.modes) {
        if (matches_kde_mode(*mode, requested_width, requested_height, requested_mhz)) {
          return mode.get();
        }
      }
      return nullptr;
    }

    kde_output_t *resolve_kde_output(const std::string &target_output_name, int requested_width, int requested_height) const {
      auto active = [](const kde_output_t &output) {
        return output.enabled && !output.removed;
      };
      auto target_match = [&](const kde_output_t &output) {
        return output.name == target_output_name ||
               output.model == target_output_name ||
               output.make == target_output_name ||
               (!target_output_name.empty() && contains_case_insensitive(kde_output_label(output), target_output_name));
      };

      for (const auto &[_, output] : kde_outputs) {
        if (active(*output) && target_match(*output) && kde_output_matches_resolution(*output, requested_width, requested_height)) {
          return output.get();
        }
      }

      for (const auto &[_, output] : kde_outputs) {
        if (active(*output) && target_match(*output)) {
          return output.get();
        }
      }

      kde_output_t *virtual_match = nullptr;
      for (const auto &[_, output] : kde_outputs) {
        if (!active(*output) || !is_kde_virtual_output(*output) || !kde_output_matches_resolution(*output, requested_width, requested_height)) {
          continue;
        }
        virtual_match = output.get();
      }
      if (virtual_match) {
        return virtual_match;
      }

      kde_output_t *single_virtual = nullptr;
      int virtual_count = 0;
      for (const auto &[_, output] : kde_outputs) {
        if (active(*output) && is_kde_virtual_output(*output)) {
          single_virtual = output.get();
          ++virtual_count;
        }
      }
      return virtual_count == 1 ? single_virtual : nullptr;
    }

    std::string describe_kde_outputs() const {
      std::ostringstream out;
      bool first = true;
      for (const auto &[_, output] : kde_outputs) {
        if (output->removed) {
          continue;
        }
        if (!first) {
          out << "; ";
        }
        first = false;
        out << kde_output_label(*output)
            << " enabled=" << (output->enabled ? "true" : "false")
            << " mode=" << describe_kde_mode(current_mode_for(*output))
            << " hdr=" << (output->hdr_enabled ? "true" : "false")
            << " caps=0x" << std::hex << output->capabilities << std::dec;
      }
      return first ? "none" : out.str();
    }

    template <typename Predicate>
    bool dispatch_until(Predicate predicate, std::chrono::milliseconds timeout, std::string &dispatch_error) {
      const auto deadline = std::chrono::steady_clock::now() + timeout;
      while (!predicate()) {
        if (wl_display_dispatch_pending(display) < 0) {
          dispatch_error = "wl_display_dispatch_pending failed";
          return false;
        }
        if (predicate()) {
          break;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
          break;
        }

        wl_display_flush(display);
        pollfd pfd {};
        pfd.fd = wl_display_get_fd(display);
        pfd.events = POLLIN;

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        const auto poll_ms = static_cast<int>(std::min<std::chrono::milliseconds>(remaining, 100ms).count());
        const auto poll_result = poll(&pfd, 1, poll_ms);
        if (poll_result < 0) {
          dispatch_error = std::string("poll failed: ") + std::strerror(errno);
          return false;
        }
        if (poll_result > 0 && (pfd.revents & POLLIN)) {
          if (wl_display_dispatch(display) < 0) {
            dispatch_error = "wl_display_dispatch failed";
            return false;
          }
        }
      }
      return predicate();
    }

    kde_output_t *wait_for_kde_output(
      const std::string &target_output_name,
      int requested_width,
      int requested_height,
      std::chrono::milliseconds timeout
    ) {
      std::string dispatch_error;
      dispatch_until(
        [&] {
          return resolve_kde_output(target_output_name, requested_width, requested_height) != nullptr;
        },
        timeout,
        dispatch_error);

      if (!dispatch_error.empty()) {
        BOOST_LOG(warning) << "KWin output management: " << dispatch_error;
      }
      return resolve_kde_output(target_output_name, requested_width, requested_height);
    }

    struct config_state_t {
      bool done = false;
      bool applied = false;
      std::string failure_reason;
    };

    template <typename Configure>
    bool apply_kde_configuration(const std::string &description, Configure configure, std::chrono::milliseconds timeout) {
      if (!output_management) {
        return false;
      }

      auto *configuration = kde_output_management_v2_create_configuration(output_management);
      if (!configuration) {
        BOOST_LOG(warning) << "KWin output management: failed to create configuration for " << description;
        return false;
      }

      config_state_t state;
      kde_output_configuration_v2_add_listener(configuration, &configuration_listener, &state);
      configure(configuration);
      kde_output_configuration_v2_apply(configuration);
      wl_display_flush(display);

      std::string dispatch_error;
      const auto completed = dispatch_until([&] { return state.done; }, timeout, dispatch_error);
      if (!dispatch_error.empty()) {
        BOOST_LOG(warning) << "KWin output management: " << dispatch_error << " while applying " << description;
      }

      const auto applied = completed && state.applied;
      if (!applied) {
        BOOST_LOG(warning)
          << "KWin output management: failed to apply " << description
          << (state.failure_reason.empty() ? "" : std::string(": ") + state.failure_reason);
      }

      kde_output_configuration_v2_destroy(configuration);
      (void)wl_display_roundtrip(display);
      return applied;
    }

    bool add_custom_kde_mode(kde_output_t &output, int requested_width, int requested_height, uint32_t requested_mhz) {
      if (!output_management || output_management_version < KDE_OUTPUT_CONFIGURATION_V2_SET_CUSTOM_MODES_SINCE_VERSION) {
        BOOST_LOG(warning) << "KWin output management: custom modes not supported by bound protocol version " << output_management_version;
        return false;
      }
      if (!(output.capabilities & KDE_OUTPUT_DEVICE_V2_CAPABILITY_CUSTOM_MODES)) {
        BOOST_LOG(warning) << "KWin output management: output '" << kde_output_label(output) << "' does not advertise custom mode support";
        return false;
      }

      auto *mode_list = kde_output_management_v2_create_mode_list(output_management);
      if (!mode_list) {
        BOOST_LOG(warning) << "KWin output management: failed to create custom mode list";
        return false;
      }

      kde_mode_list_v2_set_resolution(mode_list, static_cast<uint32_t>(requested_width), static_cast<uint32_t>(requested_height));
      kde_mode_list_v2_set_refresh_rate(mode_list, requested_mhz);
      kde_mode_list_v2_set_reduced_blanking(mode_list, 1);
      kde_mode_list_v2_add_mode(mode_list);

      BOOST_LOG(info)
        << "KWin output management: adding custom mode "
        << format_mode(requested_width, requested_height, static_cast<double>(requested_mhz) / 1000.0)
        << " to output '" << kde_output_label(output) << "'";

      const auto applied = apply_kde_configuration(
        "custom mode list",
        [&](kde_output_configuration_v2 *configuration) {
          kde_output_configuration_v2_set_custom_modes(configuration, output.device, mode_list);
        },
        1500ms);

      kde_mode_list_v2_destroy(mode_list);
      return applied;
    }

    bool apply_mode_and_hdr(kde_output_t &output, kde_mode_t *mode, bool requested_hdr) {
      const auto description = std::string("mode/HDR for ") + kde_output_label(output);
      return apply_kde_configuration(
        description,
        [&](kde_output_configuration_v2 *configuration) {
          if (mode && mode->mode) {
            kde_output_configuration_v2_mode(configuration, output.device, mode->mode);
          }
          if (requested_hdr) {
            if (output.capabilities & KDE_OUTPUT_DEVICE_V2_CAPABILITY_HIGH_DYNAMIC_RANGE) {
              kde_output_configuration_v2_set_high_dynamic_range(configuration, output.device, 1);
              hdr_configured = true;
            } else {
              BOOST_LOG(warning) << "KWin output management: output '" << kde_output_label(output) << "' does not advertise HDR support";
            }
            if (output.capabilities & KDE_OUTPUT_DEVICE_V2_CAPABILITY_WIDE_COLOR_GAMUT) {
              kde_output_configuration_v2_set_wide_color_gamut(configuration, output.device, 1);
            }
          }
        },
        1500ms);
    }

    bool apply_output_management_settings(
      const std::string &target_output_name,
      int requested_width,
      int requested_height,
      double requested_framerate,
      bool requested_hdr,
      std::chrono::milliseconds timeout
    ) {
      const auto requested_mhz = refresh_millihz(requested_framerate);
      if (!output_management || !output_device_registry) {
        BOOST_LOG(info) << "KWin output management: protocol unavailable; falling back to kscreen-doctor";
        return false;
      }
      if (requested_width <= 0 || requested_height <= 0 || requested_mhz == 0) {
        return false;
      }

      auto *output = wait_for_kde_output(target_output_name, requested_width, requested_height, timeout);
      if (!output) {
        BOOST_LOG(warning)
          << "KWin output management: could not resolve output '" << target_output_name
          << "' for " << format_mode(requested_width, requested_height, static_cast<double>(requested_mhz) / 1000.0)
          << "; known outputs: " << describe_kde_outputs();
        return false;
      }

      BOOST_LOG(info)
        << "KWin output management: resolved output '" << kde_output_label(*output)
        << "' current mode " << describe_kde_mode(current_mode_for(*output))
        << ", hdr=" << (output->hdr_enabled ? "true" : "false")
        << ", caps=0x" << std::hex << output->capabilities << std::dec;

      auto *target_mode = find_kde_mode(*output, requested_width, requested_height, requested_mhz);
      if (!target_mode) {
        add_custom_kde_mode(*output, requested_width, requested_height, requested_mhz);
        std::string dispatch_error;
        dispatch_until(
          [&] {
            return find_kde_mode(*output, requested_width, requested_height, requested_mhz) != nullptr;
          },
          1500ms,
          dispatch_error);
        if (!dispatch_error.empty()) {
          BOOST_LOG(warning) << "KWin output management: " << dispatch_error << " while waiting for custom mode";
        }
        target_mode = find_kde_mode(*output, requested_width, requested_height, requested_mhz);
      }

      if (!target_mode) {
        BOOST_LOG(warning)
          << "KWin output management: requested mode "
          << format_mode(requested_width, requested_height, static_cast<double>(requested_mhz) / 1000.0)
          << " is not advertised by output '" << kde_output_label(*output) << "'";
        return false;
      }

      BOOST_LOG(info)
        << "KWin output management: applying mode " << describe_kde_mode(target_mode)
        << " to output '" << kde_output_label(*output) << "'"
        << (requested_hdr ? " with HDR requested" : "");

      if (!apply_mode_and_hdr(*output, target_mode, requested_hdr)) {
        return false;
      }

      std::string dispatch_error;
      dispatch_until(
        [&] {
          auto *current = current_mode_for(*output);
          return current && matches_kde_mode(*current, requested_width, requested_height, requested_mhz) &&
                 (!requested_hdr || output->hdr_enabled || !(output->capabilities & KDE_OUTPUT_DEVICE_V2_CAPABILITY_HIGH_DYNAMIC_RANGE));
        },
        1500ms,
        dispatch_error);
      if (!dispatch_error.empty()) {
        BOOST_LOG(warning) << "KWin output management: " << dispatch_error << " while verifying mode/HDR";
      }

      auto *current = current_mode_for(*output);
      const auto mode_ok = current && matches_kde_mode(*current, requested_width, requested_height, requested_mhz);
      hdr_confirmed = requested_hdr && output->hdr_enabled;

      if (mode_ok) {
        BOOST_LOG(info)
          << "KWin output management: verified mode " << describe_kde_mode(current)
          << " on output '" << kde_output_label(*output) << "'"
          << (requested_hdr ? (hdr_confirmed ? ", HDR enabled" : ", HDR not confirmed") : "");
      } else {
        BOOST_LOG(warning)
          << "KWin output management: mode verification failed for output '" << kde_output_label(*output)
          << "', current mode " << describe_kde_mode(current)
          << ", requested " << format_mode(requested_width, requested_height, static_cast<double>(requested_mhz) / 1000.0);
      }

      if (requested_hdr && !hdr_confirmed) {
        BOOST_LOG(warning)
          << "KWin output management: HDR requested by client but not confirmed by KWin"
          << (hdr_configured ? "" : " (HDR capability missing or rejected)");
      }

      return mode_ok;
    }

    static void apply_refresh_rate(
      const std::string &target_output_name,
      int requested_width,
      int requested_height,
      double requested_framerate
    ) {
      const auto requested_hz = normalize_refresh_hz(requested_framerate);
      if (requested_hz <= 0.0 || requested_width <= 0 || requested_height <= 0) {
        return;
      }

      const auto requested_mode = format_mode(requested_width, requested_height, requested_hz);
      std::string last_output;
      auto resolved = wait_for_kscreen_output(
        target_output_name,
        requested_width,
        requested_height,
        3000ms,
        last_output);

      if (!resolved) {
        BOOST_LOG(warning)
          << "KWin direct capture: could not resolve KScreen output for '"
          << target_output_name << "' requested mode " << requested_mode
          << "; kscreen-doctor -o output: " << last_output;
        return;
      }

      BOOST_LOG(info)
        << "KWin direct capture: resolved KScreen output '" << resolved->name
        << "' for '" << target_output_name
        << "' current mode " << describe_mode(*resolved)
        << ", requested mode " << requested_mode;

      std::ostringstream mode_arg;
      mode_arg << "output." << resolved->name << ".mode." << requested_mode;

      BOOST_LOG(info) << "KWin direct capture: setting refresh rate via " << mode_arg.str();
      const auto mode_res = sonnenschein::vdisplay::run_args({"kscreen-doctor", mode_arg.str()}, "kscreen-doctor mode set");
      if (!mode_res.ok) {
        BOOST_LOG(warning)
          << "KWin direct capture: mode set failed for output '" << resolved->name
          << "' requested mode " << requested_mode
          << ": " << mode_res.output;
        return;
      }

      std::string verify_output;
      auto verified = wait_for_verified_mode(
        resolved->name,
        requested_width,
        requested_height,
        requested_hz,
        1500ms,
        verify_output);

      if (verified && matches_mode(*verified, requested_width, requested_height, requested_hz)) {
        BOOST_LOG(info)
          << "KWin direct capture: verified mode " << requested_mode
          << " on KScreen output '" << verified->name << "'";
        return;
      }

      BOOST_LOG(warning)
        << "KWin direct capture: mode set did not verify for output '" << resolved->name
        << "' requested mode " << requested_mode
        << ", current mode " << (verified ? describe_mode(*verified) : std::string("unknown"))
        << "; kscreen-doctor -o output: " << verify_output;
    }

    static std::string env_string(const char *key) {
      const char *value = std::getenv(key);
      return value ? value : "";
    }

    static std::filesystem::path home_dir() {
      if (auto home = env_string("HOME"); !home.empty()) {
        return home;
      }
      if (auto *pw = getpwuid(geteuid())) {
        return pw->pw_dir;
      }
      return {};
    }

    static std::filesystem::path user_applications_dir() {
      if (auto data_home = env_string("XDG_DATA_HOME"); !data_home.empty()) {
        return std::filesystem::path(data_home) / "applications";
      }
      auto home = home_dir();
      if (home.empty()) {
        return {};
      }
      return home / ".local" / "share" / "applications";
    }

    static std::string executable_path() {
      std::array<char, PATH_MAX> path {};
      const auto len = readlink("/proc/self/exe", path.data(), path.size() - 1);
      if (len <= 0) {
        return {};
      }
      return {path.data(), static_cast<std::size_t>(len)};
    }

    static void setup_permission() {
      static bool initialized = false;
      if (initialized) {
        return;
      }
      initialized = true;

      if (env_string("KWIN_WAYLAND_NO_PERMISSION_CHECKS") == "1") {
        BOOST_LOG(info) << "KWin direct capture: permission checks disabled by environment"sv;
        return;
      }

      const auto applications = user_applications_dir();
      const auto exe = executable_path();
      if (applications.empty() || exe.empty()) {
        BOOST_LOG(warning) << "KWin direct capture: cannot prepare permission desktop file"sv;
        return;
      }

      std::error_code ec;
      std::filesystem::create_directories(applications, ec);
      if (ec) {
        BOOST_LOG(warning) << "KWin direct capture: failed to create "sv
                           << applications.string() << ": "sv << ec.message();
        return;
      }

      const auto path = applications / "sonnenschein-kwin-screencast.desktop";

      std::ofstream file(path);
      if (!file.is_open()) {
        BOOST_LOG(warning) << "KWin direct capture: failed to write permission file "sv << path.string();
        return;
      }

      file << "[Desktop Entry]\n"
           << "Exec=" << exe << "\n"
           << "X-KDE-Wayland-Interfaces=zkde_screencast_unstable_v1,kde_output_management_v2,kde_output_device_registry_v2\n"
           << "Type=Application\n"
           << "Name=sonnenschein-kwin-wayland-permission\n"
           << "Comment=Sonnenschein KWin screencast permission\n"
           << "NoDisplay=true\n";
      file.close();

      BOOST_LOG(info) << "KWin direct capture: created permission file "sv
                      << path.string() << "; waiting for KWin to rescan"sv;
      std::this_thread::sleep_for(3s);
    }

    bool wait_for_stream() {
      auto deadline = std::chrono::steady_clock::now() + 5s;
      while (!ready && !failed && std::chrono::steady_clock::now() < deadline) {
        wl_display_flush(display);
        pollfd pfd {};
        pfd.fd = wl_display_get_fd(display);
        pfd.events = POLLIN;

        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
          break;
        }

        if (poll(&pfd, 1, static_cast<int>(remaining.count())) > 0 && (pfd.revents & POLLIN)) {
          if (wl_display_dispatch(display) < 0) {
            error = "wl_display_dispatch failed";
            return false;
          }
        }
      }
      return ready && !failed;
    }

    static void on_registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
      auto *self = static_cast<kwin_session_t *>(data);
      if (!std::strcmp(interface, zkde_screencast_unstable_v1_interface.name)) {
        const auto bind_version = std::min(version, static_cast<uint32_t>(6));
        self->screencast = static_cast<zkde_screencast_unstable_v1 *>(
          wl_registry_bind(registry, name, &zkde_screencast_unstable_v1_interface, bind_version));
        BOOST_LOG(info) << "KWin direct capture: bound zkde_screencast_unstable_v1 version "sv << bind_version;
      } else if (!std::strcmp(interface, kde_output_management_v2_interface.name)) {
        const auto bind_version = std::min(version, static_cast<uint32_t>(21));
        self->output_management_version = bind_version;
        self->output_management = static_cast<kde_output_management_v2 *>(
          wl_registry_bind(registry, name, &kde_output_management_v2_interface, bind_version));
        BOOST_LOG(info) << "KWin output management: bound kde_output_management_v2 version "sv << bind_version;
      } else if (!std::strcmp(interface, kde_output_device_registry_v2_interface.name)) {
        if (version < 21) {
          BOOST_LOG(warning) << "KWin output management: ignoring kde_output_device_registry_v2 version "sv
                             << version << " because version 21+ is required";
          return;
        }
        const auto bind_version = std::min(version, static_cast<uint32_t>(23));
        self->output_device_registry = static_cast<kde_output_device_registry_v2 *>(
          wl_registry_bind(registry, name, &kde_output_device_registry_v2_interface, bind_version));
        kde_output_device_registry_v2_add_listener(self->output_device_registry, &output_device_registry_listener, self);
        BOOST_LOG(info) << "KWin output management: bound kde_output_device_registry_v2 version "sv << bind_version;
      } else if (!std::strcmp(interface, wl_output_interface.name)) {
        const auto bind_version = std::min(version, static_cast<uint32_t>(4));
        auto *output = static_cast<wl_output *>(
          wl_registry_bind(registry, name, &wl_output_interface, bind_version));
        self->outputs.emplace(output, output_t{output});
        wl_output_add_listener(output, &output_listener, self);
      }
    }

    static void on_registry_global_remove(void *, wl_registry *, uint32_t) {}

    static constexpr wl_registry_listener registry_listener {
      .global = on_registry_global,
      .global_remove = on_registry_global_remove,
    };

    static void on_output_geometry(void *data, wl_output *output, int32_t x, int32_t y, int32_t, int32_t, int32_t, const char *, const char *, int32_t) {
      auto *self = static_cast<kwin_session_t *>(data);
      auto &params = self->outputs.at(output);
      params.x = x;
      params.y = y;
    }

    static void on_output_mode(void *data, wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t) {
      if (!(flags & WL_OUTPUT_MODE_CURRENT)) {
        return;
      }
      auto *self = static_cast<kwin_session_t *>(data);
      auto &params = self->outputs.at(output);
      params.width = width;
      params.height = height;
    }

    static void on_output_done(void *, wl_output *) {}
    static void on_output_scale(void *, wl_output *, int32_t) {}

    static void on_output_name(void *data, wl_output *output, const char *name) {
      auto *self = static_cast<kwin_session_t *>(data);
      self->outputs.at(output).name = name ? name : "";
    }

    static void on_output_description(void *data, wl_output *output, const char *description) {
      auto *self = static_cast<kwin_session_t *>(data);
      self->outputs.at(output).description = description ? description : "";
    }

    static constexpr wl_output_listener output_listener {
      .geometry = on_output_geometry,
      .mode = on_output_mode,
      .done = on_output_done,
      .scale = on_output_scale,
      .name = on_output_name,
      .description = on_output_description,
    };

    static void on_output_device_registry_finished(void *, kde_output_device_registry_v2 *) {}

    static void on_output_device_registry_output(
      void *data,
      kde_output_device_registry_v2 *,
      kde_output_device_v2 *device
    ) {
      auto *self = static_cast<kwin_session_t *>(data);
      auto output = std::make_unique<kde_output_t>();
      output->device = device;
      auto *raw = output.get();
      self->kde_outputs.emplace(device, std::move(output));
      kde_output_device_v2_add_listener(device, &kde_output_device_listener, self);
      BOOST_LOG(debug) << "KWin output management: announced output device "sv << raw;
    }

    static constexpr kde_output_device_registry_v2_listener output_device_registry_listener {
      .finished = on_output_device_registry_finished,
      .output = on_output_device_registry_output,
    };

    static kde_output_t *kde_output_for(kwin_session_t *self, kde_output_device_v2 *device) {
      const auto it = self->kde_outputs.find(device);
      return it == self->kde_outputs.end() ? nullptr : it->second.get();
    }

    static void on_kde_output_geometry(
      void *data,
      kde_output_device_v2 *device,
      int32_t x,
      int32_t y,
      int32_t,
      int32_t,
      int32_t,
      const char *make,
      const char *model,
      int32_t
    ) {
      auto *self = static_cast<kwin_session_t *>(data);
      if (auto *output = kde_output_for(self, device)) {
        output->x = x;
        output->y = y;
        output->make = make ? make : "";
        output->model = model ? model : "";
      }
    }

    static void on_kde_output_current_mode(void *data, kde_output_device_v2 *device, kde_output_device_mode_v2 *mode) {
      auto *self = static_cast<kwin_session_t *>(data);
      if (auto *output = kde_output_for(self, device)) {
        output->current_mode = mode;
      }
    }

    static void on_kde_output_mode(void *data, kde_output_device_v2 *device, kde_output_device_mode_v2 *mode) {
      auto *self = static_cast<kwin_session_t *>(data);
      auto *output = kde_output_for(self, device);
      if (!output) {
        kde_output_device_mode_v2_destroy(mode);
        return;
      }

      auto info = std::make_unique<kde_mode_t>();
      info->mode = mode;
      auto *raw = info.get();
      output->modes.push_back(std::move(info));
      self->kde_modes[mode] = raw;
      kde_output_device_mode_v2_add_listener(mode, &kde_mode_listener, raw);
    }

    static void on_kde_output_done(void *, kde_output_device_v2 *) {}
    static void on_kde_output_scale(void *, kde_output_device_v2 *, wl_fixed_t) {}
    static void on_kde_output_edid(void *, kde_output_device_v2 *, const char *) {}

    static void on_kde_output_enabled(void *data, kde_output_device_v2 *device, int32_t enabled) {
      auto *self = static_cast<kwin_session_t *>(data);
      if (auto *output = kde_output_for(self, device)) {
        output->enabled = enabled != 0;
      }
    }

    static void on_kde_output_uuid(void *, kde_output_device_v2 *, const char *) {}
    static void on_kde_output_serial_number(void *, kde_output_device_v2 *, const char *) {}
    static void on_kde_output_eisa_id(void *, kde_output_device_v2 *, const char *) {}

    static void on_kde_output_capabilities(void *data, kde_output_device_v2 *device, uint32_t flags) {
      auto *self = static_cast<kwin_session_t *>(data);
      if (auto *output = kde_output_for(self, device)) {
        output->capabilities = flags;
      }
    }

    static void on_kde_output_overscan(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_vrr_policy(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_rgb_range(void *, kde_output_device_v2 *, uint32_t) {}

    static void on_kde_output_name(void *data, kde_output_device_v2 *device, const char *name) {
      auto *self = static_cast<kwin_session_t *>(data);
      if (auto *output = kde_output_for(self, device)) {
        output->name = name ? name : "";
      }
    }

    static void on_kde_output_high_dynamic_range(void *data, kde_output_device_v2 *device, uint32_t enabled) {
      auto *self = static_cast<kwin_session_t *>(data);
      if (auto *output = kde_output_for(self, device)) {
        output->hdr_enabled = enabled != 0;
      }
    }

    static void on_kde_output_sdr_brightness(void *, kde_output_device_v2 *, uint32_t) {}

    static void on_kde_output_wide_color_gamut(void *data, kde_output_device_v2 *device, uint32_t enabled) {
      auto *self = static_cast<kwin_session_t *>(data);
      if (auto *output = kde_output_for(self, device)) {
        output->wcg_enabled = enabled != 0;
      }
    }

    static void on_kde_output_auto_rotate_policy(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_icc_profile_path(void *, kde_output_device_v2 *, const char *) {}
    static void on_kde_output_brightness_metadata(void *, kde_output_device_v2 *, uint32_t, uint32_t, uint32_t) {}
    static void on_kde_output_brightness_overrides(void *, kde_output_device_v2 *, int32_t, int32_t, int32_t) {}
    static void on_kde_output_sdr_gamut_wideness(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_color_profile_source(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_brightness(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_color_power_tradeoff(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_dimming(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_replication_source(void *, kde_output_device_v2 *, const char *) {}
    static void on_kde_output_ddc_ci_allowed(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_max_bits_per_color(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_max_bits_per_color_range(void *, kde_output_device_v2 *, uint32_t, uint32_t) {}
    static void on_kde_output_automatic_max_bits_per_color_limit(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_edr_policy(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_sharpness(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_priority(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_auto_brightness(void *, kde_output_device_v2 *, uint32_t) {}

    static void on_kde_output_removed(void *data, kde_output_device_v2 *device) {
      auto *self = static_cast<kwin_session_t *>(data);
      if (auto *output = kde_output_for(self, device)) {
        output->removed = true;
      }
    }

    static void on_kde_output_hdr_icc_profile_path(void *, kde_output_device_v2 *, const char *) {}
    static void on_kde_output_hdr_color_profile_source(void *, kde_output_device_v2 *, uint32_t) {}
    static void on_kde_output_abm_level(void *, kde_output_device_v2 *, uint32_t) {}

    static constexpr kde_output_device_v2_listener kde_output_device_listener {
      .geometry = on_kde_output_geometry,
      .current_mode = on_kde_output_current_mode,
      .mode = on_kde_output_mode,
      .done = on_kde_output_done,
      .scale = on_kde_output_scale,
      .edid = on_kde_output_edid,
      .enabled = on_kde_output_enabled,
      .uuid = on_kde_output_uuid,
      .serial_number = on_kde_output_serial_number,
      .eisa_id = on_kde_output_eisa_id,
      .capabilities = on_kde_output_capabilities,
      .overscan = on_kde_output_overscan,
      .vrr_policy = on_kde_output_vrr_policy,
      .rgb_range = on_kde_output_rgb_range,
      .name = on_kde_output_name,
      .high_dynamic_range = on_kde_output_high_dynamic_range,
      .sdr_brightness = on_kde_output_sdr_brightness,
      .wide_color_gamut = on_kde_output_wide_color_gamut,
      .auto_rotate_policy = on_kde_output_auto_rotate_policy,
      .icc_profile_path = on_kde_output_icc_profile_path,
      .brightness_metadata = on_kde_output_brightness_metadata,
      .brightness_overrides = on_kde_output_brightness_overrides,
      .sdr_gamut_wideness = on_kde_output_sdr_gamut_wideness,
      .color_profile_source = on_kde_output_color_profile_source,
      .brightness = on_kde_output_brightness,
      .color_power_tradeoff = on_kde_output_color_power_tradeoff,
      .dimming = on_kde_output_dimming,
      .replication_source = on_kde_output_replication_source,
      .ddc_ci_allowed = on_kde_output_ddc_ci_allowed,
      .max_bits_per_color = on_kde_output_max_bits_per_color,
      .max_bits_per_color_range = on_kde_output_max_bits_per_color_range,
      .automatic_max_bits_per_color_limit = on_kde_output_automatic_max_bits_per_color_limit,
      .edr_policy = on_kde_output_edr_policy,
      .sharpness = on_kde_output_sharpness,
      .priority = on_kde_output_priority,
      .auto_brightness = on_kde_output_auto_brightness,
      .removed = on_kde_output_removed,
      .hdr_icc_profile_path = on_kde_output_hdr_icc_profile_path,
      .hdr_color_profile_source = on_kde_output_hdr_color_profile_source,
      .abm_level = on_kde_output_abm_level,
    };

    static void on_kde_mode_size(void *data, kde_output_device_mode_v2 *, int32_t width, int32_t height) {
      auto *mode = static_cast<kde_mode_t *>(data);
      mode->width = width;
      mode->height = height;
    }

    static void on_kde_mode_refresh(void *data, kde_output_device_mode_v2 *, int32_t refresh) {
      auto *mode = static_cast<kde_mode_t *>(data);
      mode->refresh_mhz = refresh;
    }

    static void on_kde_mode_preferred(void *data, kde_output_device_mode_v2 *) {
      auto *mode = static_cast<kde_mode_t *>(data);
      mode->preferred = true;
    }

    static void on_kde_mode_removed(void *data, kde_output_device_mode_v2 *) {
      auto *mode = static_cast<kde_mode_t *>(data);
      mode->removed = true;
    }

    static void on_kde_mode_flags(void *data, kde_output_device_mode_v2 *, uint32_t flags) {
      auto *mode = static_cast<kde_mode_t *>(data);
      mode->flags = flags;
    }

    static constexpr kde_output_device_mode_v2_listener kde_mode_listener {
      .size = on_kde_mode_size,
      .refresh = on_kde_mode_refresh,
      .preferred = on_kde_mode_preferred,
      .removed = on_kde_mode_removed,
      .flags = on_kde_mode_flags,
    };

    static void on_configuration_applied(void *data, kde_output_configuration_v2 *) {
      auto *state = static_cast<config_state_t *>(data);
      state->done = true;
      state->applied = true;
    }

    static void on_configuration_failed(void *data, kde_output_configuration_v2 *) {
      auto *state = static_cast<config_state_t *>(data);
      state->done = true;
      state->applied = false;
    }

    static void on_configuration_failure_reason(void *data, kde_output_configuration_v2 *, const char *reason) {
      auto *state = static_cast<config_state_t *>(data);
      state->failure_reason = reason ? reason : "";
    }

    static constexpr kde_output_configuration_v2_listener configuration_listener {
      .applied = on_configuration_applied,
      .failed = on_configuration_failed,
      .failure_reason = on_configuration_failure_reason,
    };

    static void on_stream_closed(void *data, zkde_screencast_stream_unstable_v1 *) {
      auto *self = static_cast<kwin_session_t *>(data);
      self->failed = true;
      self->error = "stream closed by KWin";
    }

    static void on_stream_created(void *data, zkde_screencast_stream_unstable_v1 *, uint32_t node) {
      auto *self = static_cast<kwin_session_t *>(data);
      self->node_id = node;
      self->ready = true;
    }

    static void on_stream_failed(void *data, zkde_screencast_stream_unstable_v1 *, const char *message) {
      auto *self = static_cast<kwin_session_t *>(data);
      self->failed = true;
      self->error = message ? message : "stream_output failed";
    }

    static void on_stream_serial(void *, zkde_screencast_stream_unstable_v1 *, uint32_t, uint32_t) {}

    static constexpr zkde_screencast_stream_unstable_v1_listener stream_listener {
      .closed = on_stream_closed,
      .created = on_stream_created,
      .failed = on_stream_failed,
      .serial = on_stream_serial,
    };
  };
#endif

  /**
   * Portal session: manages D-Bus interaction with xdg-desktop-portal.
   */
  struct portal_session_t {
    GDBusConnection *conn = nullptr;
    std::string session_handle;
    std::string restore_token;
    uint32_t pw_node_id = 0;
    int pw_fd = -1;
    uint32_t available_source_types = 0;
    uint32_t available_cursor_modes = 0;
    bool ready = false;

    // Synchronization for async D-Bus calls
    std::mutex mtx;
    std::condition_variable cv;
    bool response_received = false;
    uint32_t response_code = 0;
    GVariant *response_results = nullptr;

    ~portal_session_t() {
      if (pw_fd >= 0) close(pw_fd);
      if (conn) g_object_unref(conn);
    }

    static void on_response(GDBusConnection *, const gchar *, const gchar *,
                            const gchar *, const gchar *, GVariant *params,
                            gpointer user_data) {
      auto *self = static_cast<portal_session_t *>(user_data);
      std::lock_guard lk(self->mtx);

      g_variant_get(params, "(u@a{sv})", &self->response_code, &self->response_results);
      self->response_received = true;
      self->cv.notify_all();
    }

    bool wait_response(guint signal_id, int timeout_ms = 10000) {
      response_received = false;
      response_code = 0;
      if (response_results) { g_variant_unref(response_results); response_results = nullptr; }

      // We must pump the GLib main context for D-Bus signal delivery.
      // g_dbus_signal callbacks are dispatched on the default main context,
      // so we iterate it in a polling loop until the response arrives.
      auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
      while (!response_received && std::chrono::steady_clock::now() < deadline) {
        g_main_context_iteration(nullptr, FALSE);  // non-blocking pump
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      g_dbus_connection_signal_unsubscribe(conn, signal_id);

      if (!response_received) {
        BOOST_LOG(error) << "Portal: D-Bus response timed out"sv;
        return false;
      }
      if (response_code != 0) {
        BOOST_LOG(error) << "Portal: request denied (code " << response_code << ")"sv;
        return false;
      }
      return true;
    }

    guint subscribe_response(const std::string &request_path) {
      return g_dbus_connection_signal_subscribe(
        conn, PORTAL_BUS_NAME, PORTAL_REQUEST, "Response",
        request_path.c_str(), nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        on_response, this, nullptr);
    }

    bool init() {
      GError *err = nullptr;
      conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
      if (!conn) {
        BOOST_LOG(error) << "Portal: can't connect to session bus: "sv << (err ? err->message : "unknown");
        if (err) g_error_free(err);
        return false;
      }
      available_source_types = get_uint_property("AvailableSourceTypes");
      available_cursor_modes = get_uint_property("AvailableCursorModes");
      BOOST_LOG(info) << "Portal: available source types="sv << available_source_types
                      << " cursor modes="sv << available_cursor_modes;
      return true;
    }

    uint32_t get_uint_property(const char *name) {
      GError *err = nullptr;
      auto result = g_dbus_connection_call_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, "org.freedesktop.DBus.Properties",
        "Get", g_variant_new("(ss)", PORTAL_SCREENCAST, name),
        G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &err);
      if (!result) {
        BOOST_LOG(warning) << "Portal: failed to read property "sv << name << ": "sv
                           << (err ? err->message : "unknown");
        if (err) g_error_free(err);
        return 0;
      }

      GVariant *value = nullptr;
      g_variant_get(result, "(@v)", &value);
      uint32_t out = 0;
      if (value) {
        GVariant *inner = g_variant_get_variant(value);
        if (inner && g_variant_is_of_type(inner, G_VARIANT_TYPE_UINT32)) {
          out = g_variant_get_uint32(inner);
        }
        if (inner) g_variant_unref(inner);
        g_variant_unref(value);
      }
      g_variant_unref(result);
      return out;
    }

    bool create_session() {
      auto token = make_token();
      auto req_path = make_request_path(conn, token);
      auto sig = subscribe_response(req_path);

      auto opts = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
      g_variant_builder_add(opts, "{sv}", "handle_token", g_variant_new_string(token.c_str()));
      g_variant_builder_add(opts, "{sv}", "session_handle_token", g_variant_new_string(make_token().c_str()));

      GError *err = nullptr;
      auto result = g_dbus_connection_call_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, PORTAL_SCREENCAST,
        "CreateSession", g_variant_new("(a{sv})", opts), nullptr,
        G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &err);
      g_variant_builder_unref(opts);

      if (!result) {
        BOOST_LOG(error) << "Portal: CreateSession failed: "sv << (err ? err->message : "unknown");
        if (err) g_error_free(err);
        g_dbus_connection_signal_unsubscribe(conn, sig);
        return false;
      }
      g_variant_unref(result);

      if (!wait_response(sig)) return false;

      // Extract session handle
      if (response_results) {
        GVariant *handle = g_variant_lookup_value(response_results, "session_handle", G_VARIANT_TYPE_STRING);
        if (handle) {
          session_handle = g_variant_get_string(handle, nullptr);
          g_variant_unref(handle);
          BOOST_LOG(info) << "Portal: session created: "sv << session_handle;
        }
      }
      return !session_handle.empty();
    }

    bool select_sources() {
      auto token = make_token();
      auto req_path = make_request_path(conn, token);
      auto sig = subscribe_response(req_path);

      auto opts = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
      g_variant_builder_add(opts, "{sv}", "handle_token", g_variant_new_string(token.c_str()));
      g_variant_builder_add(opts, "{sv}", "types", g_variant_new_uint32(SOURCE_MONITOR));
      g_variant_builder_add(opts, "{sv}", "multiple", g_variant_new_boolean(FALSE));
      g_variant_builder_add(opts, "{sv}", "persist_mode", g_variant_new_uint32(2)); // persistent
      if (available_cursor_modes & CURSOR_EMBEDDED) {
        g_variant_builder_add(opts, "{sv}", "cursor_mode", g_variant_new_uint32(CURSOR_EMBEDDED));
        BOOST_LOG(info) << "Portal: requesting embedded cursor mode"sv;
      } else if (available_cursor_modes & CURSOR_METADATA) {
        BOOST_LOG(warning) << "Portal: cursor metadata mode is available, but PipeWire cursor metadata is not implemented yet; cursor will stay hidden"sv;
      } else if (available_cursor_modes & CURSOR_HIDDEN) {
        BOOST_LOG(warning) << "Portal: only hidden cursor mode is available"sv;
      }
      if (!restore_token.empty()) {
        g_variant_builder_add(opts, "{sv}", "restore_token", g_variant_new_string(restore_token.c_str()));
      }

      GError *err = nullptr;
      auto result = g_dbus_connection_call_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, PORTAL_SCREENCAST,
        "SelectSources", g_variant_new("(oa{sv})", session_handle.c_str(), opts),
        nullptr, G_DBUS_CALL_FLAGS_NONE, 30000, nullptr, &err);
      g_variant_builder_unref(opts);

      if (!result) {
        BOOST_LOG(error) << "Portal: SelectSources failed: "sv << (err ? err->message : "unknown");
        if (err) g_error_free(err);
        g_dbus_connection_signal_unsubscribe(conn, sig);
        return false;
      }
      g_variant_unref(result);

      return wait_response(sig);
    }

    bool start() {
      auto token = make_token();
      auto req_path = make_request_path(conn, token);
      auto sig = subscribe_response(req_path);

      auto opts = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
      g_variant_builder_add(opts, "{sv}", "handle_token", g_variant_new_string(token.c_str()));

      GError *err = nullptr;
      auto result = g_dbus_connection_call_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, PORTAL_SCREENCAST,
        "Start", g_variant_new("(osa{sv})", session_handle.c_str(), "", opts),
        nullptr, G_DBUS_CALL_FLAGS_NONE, 30000, nullptr, &err);
      g_variant_builder_unref(opts);

      if (!result) {
        BOOST_LOG(error) << "Portal: Start failed: "sv << (err ? err->message : "unknown");
        if (err) g_error_free(err);
        g_dbus_connection_signal_unsubscribe(conn, sig);
        return false;
      }
      g_variant_unref(result);

      if (!wait_response(sig)) return false;

      // Extract streams and restore_token
      if (response_results) {
        GVariant *token = g_variant_lookup_value(response_results, "restore_token", G_VARIANT_TYPE_STRING);
        if (token) {
          restore_token = g_variant_get_string(token, nullptr);
          g_variant_unref(token);
        }

        GVariant *streams = g_variant_lookup_value(response_results, "streams", nullptr);
        if (streams) {
          GVariantIter iter;
          g_variant_iter_init(&iter, streams);
          GVariant *stream_entry = nullptr;
          if ((stream_entry = g_variant_iter_next_value(&iter))) {
            GVariant *node_id_v = g_variant_get_child_value(stream_entry, 0);
            pw_node_id = g_variant_get_uint32(node_id_v);
            g_variant_unref(node_id_v);

            GVariant *props = g_variant_get_child_value(stream_entry, 1);
            log_stream_properties(props);
            g_variant_unref(props);

            g_variant_unref(stream_entry);
            BOOST_LOG(info) << "Portal: stream node_id="sv << pw_node_id;
          }
          g_variant_unref(streams);
        }
      }

      // Get PipeWire fd
      GError *fd_err = nullptr;
      GUnixFDList *fd_list = nullptr;
      auto fd_opts = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
      auto fd_result = g_dbus_connection_call_with_unix_fd_list_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, PORTAL_SCREENCAST,
        "OpenPipeWireRemote",
        g_variant_new("(oa{sv})", session_handle.c_str(), fd_opts),
        nullptr, G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &fd_list, nullptr, &fd_err);
      g_variant_builder_unref(fd_opts);

      if (!fd_result) {
        BOOST_LOG(error) << "Portal: OpenPipeWireRemote failed: "sv << (fd_err ? fd_err->message : "unknown");
        if (fd_err) g_error_free(fd_err);
        return false;
      }

      if (!fd_list) {
        BOOST_LOG(error) << "Portal: OpenPipeWireRemote returned no fd list"sv;
        g_variant_unref(fd_result);
        return false;
      }

      gint32 fd_index = 0;
      g_variant_get(fd_result, "(h)", &fd_index);
      GError *get_fd_err = nullptr;
      pw_fd = g_unix_fd_list_get(fd_list, fd_index, &get_fd_err);
      g_variant_unref(fd_result);
      g_object_unref(fd_list);

      if (pw_fd < 0) {
        BOOST_LOG(error) << "Portal: failed to get PipeWire fd: "sv << (get_fd_err ? get_fd_err->message : "unknown");
        if (get_fd_err) g_error_free(get_fd_err);
        return false;
      }

      BOOST_LOG(info) << "Portal: PipeWire fd="sv << pw_fd << " node_id="sv << pw_node_id;
      ready = (pw_fd >= 0 && pw_node_id > 0);
      return ready;
    }

    static std::string source_type_name(uint32_t source_type) {
      switch (source_type) {
        case SOURCE_MONITOR:
          return "MONITOR";
        case SOURCE_WINDOW:
          return "WINDOW";
        case SOURCE_VIRTUAL:
          return "VIRTUAL";
        default:
          return "unknown(" + std::to_string(source_type) + ")";
      }
    }

    static void log_stream_properties(GVariant *props) {
      if (!props) return;

      uint32_t source_type = 0;
      if (g_variant_lookup(props, "source_type", "u", &source_type)) {
        BOOST_LOG(info) << "Portal: stream source_type="sv << source_type_name(source_type);
        if (source_type == SOURCE_VIRTUAL) {
          BOOST_LOG(warning) << "Portal: selected XDG VIRTUAL source; KDE currently tends to expose this as 1920x1080. Prefer the existing Sonnenschein monitor source if the dialog offers it."sv;
        }
      }

      const gchar *id = nullptr;
      if (g_variant_lookup(props, "id", "&s", &id)) {
        BOOST_LOG(info) << "Portal: stream id="sv << id;
      }

      const gchar *mapping_id = nullptr;
      if (g_variant_lookup(props, "mapping_id", "&s", &mapping_id)) {
        BOOST_LOG(info) << "Portal: stream mapping_id="sv << mapping_id;
      }

      gint32 x = 0, y = 0;
      if (g_variant_lookup(props, "position", "(ii)", &x, &y)) {
        BOOST_LOG(info) << "Portal: stream position="sv << x << ","sv << y;
      }

      gint32 width = 0, height = 0;
      if (g_variant_lookup(props, "size", "(ii)", &width, &height)) {
        BOOST_LOG(info) << "Portal: stream logical size="sv << width << "x"sv << height;
      }

      guint64 serial = 0;
      if (g_variant_lookup(props, "pipewire-serial", "t", &serial)) {
        BOOST_LOG(info) << "Portal: stream pipewire-serial="sv << serial;
      }
    }
  };

  /**
   * PipeWire stream capture: connects to PipeWire and receives frames.
   */
  struct pw_capture_t {
    pw_thread_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_stream *stream = nullptr;

    uint32_t width = 0, height = 0;
    uint32_t stride = 0;
    uint32_t format = 0; // SPA format
    uint32_t requested_width = 1920;
    uint32_t requested_height = 1080;
    uint32_t requested_framerate = 60;
    bool is_dmabuf = false;

    std::mutex frame_mtx;
    std::condition_variable frame_cv;
    bool has_new_frame = false;
    bool has_frame = false;
    uint64_t frame_sequence = 0;
    double negotiated_framerate = 0.0;

    // Latest frame data (SHM path)
    std::vector<uint8_t> frame_data;

    // Latest frame DMA-BUF info
    struct dmabuf_frame_t {
      int fd = -1;
      uint32_t width = 0, height = 0;
      uint32_t stride = 0, offset = 0;
      uint32_t drm_format = 0;
    };
    dmabuf_frame_t dmabuf_frame;

    static void on_process(void *userdata) {
      auto *self = static_cast<pw_capture_t *>(userdata);
      auto *buf = pw_stream_dequeue_buffer(self->stream);
      if (!buf) return;

      auto *d = &buf->buffer->datas[0];

      if (d->type == SPA_DATA_DmaBuf) {
        std::lock_guard lk(self->frame_mtx);
        self->dmabuf_frame.fd = d->fd;
        self->dmabuf_frame.width = self->width;
        self->dmabuf_frame.height = self->height;
        self->dmabuf_frame.stride = d->chunk->stride;
        self->dmabuf_frame.offset = d->chunk->offset;
        self->is_dmabuf = true;
        self->has_frame = true;
        self->has_new_frame = true;
        ++self->frame_sequence;
        self->frame_cv.notify_all();
      }
      else if (d->type == SPA_DATA_MemFd || d->type == SPA_DATA_MemPtr) {
        auto *src = static_cast<uint8_t *>(d->data);
        if (src && d->chunk->size > 0) {
          std::lock_guard lk(self->frame_mtx);
          self->frame_data.resize(d->chunk->size);
          std::memcpy(self->frame_data.data(), src + d->chunk->offset, d->chunk->size);
          self->stride = d->chunk->stride;
          self->is_dmabuf = false;
          self->has_frame = true;
          self->has_new_frame = true;
          ++self->frame_sequence;
          self->frame_cv.notify_all();
        }
      }

      pw_stream_queue_buffer(self->stream, buf);
    }

    static void on_param_changed(void *userdata, uint32_t id, const struct spa_pod *param) {
      auto *self = static_cast<pw_capture_t *>(userdata);
      if (!param || id != SPA_PARAM_Format) return;

      struct spa_video_info vid_info;
      if (spa_format_video_raw_parse(param, &vid_info.info.raw) < 0) return;

      self->width = vid_info.info.raw.size.width;
      self->height = vid_info.info.raw.size.height;
      self->format = vid_info.info.raw.format;
      if (vid_info.info.raw.framerate.denom != 0) {
        self->negotiated_framerate = static_cast<double>(vid_info.info.raw.framerate.num) /
                                     static_cast<double>(vid_info.info.raw.framerate.denom);
      }

      BOOST_LOG(info) << "PipeWire: format negotiated "sv
                      << self->width << "x"sv << self->height
                      << " fmt="sv << self->format
                      << " fps="sv << self->negotiated_framerate;
      if (self->width != self->requested_width || self->height != self->requested_height) {
        BOOST_LOG(warning) << "PipeWire: negotiated size differs from client request "
                           << self->requested_width << "x"sv << self->requested_height
                           << " -> "sv << self->width << "x"sv << self->height;
      }
      if (self->negotiated_framerate > 0.0 &&
          self->negotiated_framerate + 0.5 < static_cast<double>(self->requested_framerate)) {
        BOOST_LOG(warning) << "PipeWire: negotiated framerate "sv << self->negotiated_framerate
                           << " is below client request "sv << self->requested_framerate
                           << "; capture will pace duplicate frames at the client rate"sv;
      }
    }

    static void on_state_changed(void *userdata, enum pw_stream_state old,
                                  enum pw_stream_state state, const char *error) {
      BOOST_LOG(info) << "PipeWire: stream state "sv
                      << pw_stream_state_as_string(old) << " -> "sv
                      << pw_stream_state_as_string(state)
                      << (error ? std::string(" (") + error + ")" : "");
    }

    static const pw_stream_events stream_events;

    bool init(int fd, uint32_t node_id, uint32_t req_width, uint32_t req_height, uint32_t req_framerate) {
      requested_width = req_width ? req_width : 1920;
      requested_height = req_height ? req_height : 1080;
      requested_framerate = normalize_framerate_hz(req_framerate);

      pw_init(nullptr, nullptr);

      loop = pw_thread_loop_new("sonnenschein-pw", nullptr);
      if (!loop) {
        BOOST_LOG(error) << "PipeWire: failed to create thread loop"sv;
        return false;
      }

      context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
      if (!context) {
        BOOST_LOG(error) << "PipeWire: failed to create context"sv;
        return false;
      }

      pw_thread_loop_lock(loop);
      pw_thread_loop_start(loop);

      if (fd >= 0) {
        core = pw_context_connect_fd(context, fcntl(fd, F_DUPFD_CLOEXEC, 3), nullptr, 0);
      } else {
        core = pw_context_connect(context, nullptr, 0);
      }
      if (!core) {
        BOOST_LOG(error) << "PipeWire: failed to connect to "sv << (fd >= 0 ? "remote" : "local core");
        pw_thread_loop_unlock(loop);
        return false;
      }

      auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Video",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Screen",
        nullptr);

      stream = pw_stream_new(core, "sonnenschein-capture", props);
      if (!stream) {
        BOOST_LOG(error) << "PipeWire: failed to create stream"sv;
        pw_thread_loop_unlock(loop);
        return false;
      }

      pw_stream_add_listener(stream, &stream_listener, &stream_events, this);

      // Build format params — prefer DMA-BUF, fallback to SHM
      uint8_t params_buf[1024];
      struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(params_buf, sizeof(params_buf));

      struct spa_rectangle default_size = SPA_RECTANGLE(requested_width, requested_height);
      struct spa_rectangle min_size = SPA_RECTANGLE(1, 1);
      struct spa_rectangle max_size = SPA_RECTANGLE(8192, 8192);
      struct spa_fraction default_rate = SPA_FRACTION(requested_framerate, 1);
      struct spa_fraction min_rate = SPA_FRACTION(0, 1);
      struct spa_fraction max_rate = SPA_FRACTION(std::max<uint32_t>(requested_framerate, 144), 1);

      BOOST_LOG(info) << "PipeWire: requesting format "
                      << requested_width << "x" << requested_height
                      << "@" << static_cast<uint32_t>(requested_framerate);

      auto *fmt = static_cast<const spa_pod *>(spa_pod_builder_add_object(
        &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType,    SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(3,
          SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx),
        SPA_FORMAT_VIDEO_size,   SPA_POD_CHOICE_RANGE_Rectangle(
          &default_size, &min_size, &max_size),
        SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
          &default_rate, &min_rate, &max_rate)
      ));

      const struct spa_pod *param_list[] = { fmt };

      pw_stream_connect(stream, PW_DIRECTION_INPUT, node_id,
        static_cast<pw_stream_flags>(
          PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        param_list, 1);

      pw_thread_loop_unlock(loop);

      BOOST_LOG(info) << "PipeWire: stream connecting to node "sv << node_id;
      return true;
    }

    void cleanup() {
      if (loop) {
        pw_thread_loop_lock(loop);
        if (stream) { pw_stream_destroy(stream); stream = nullptr; }
        if (core) { pw_core_disconnect(core); core = nullptr; }
        pw_thread_loop_unlock(loop);
        pw_thread_loop_stop(loop);
        if (context) { pw_context_destroy(context); context = nullptr; }
        pw_thread_loop_destroy(loop);
        loop = nullptr;
      }
      pw_deinit();
    }

    ~pw_capture_t() { cleanup(); }

    spa_hook stream_listener {};
  };

  const pw_stream_events pw_capture_t::stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .state_changed = pw_capture_t::on_state_changed,
    .param_changed = pw_capture_t::on_param_changed,
    .process = pw_capture_t::on_process,
  };

  // ---- platf::display_t implementation ----

  struct img_t : public platf::img_t {
    ~img_t() override {
      delete[] data;
      data = nullptr;
    }
  };

  class pw_display_t : public platf::display_t {
  public:
      int init(platf::mem_type_e hwdevice_type, const std::string &display_name, const ::video::config_t &config) {
        mem_type = hwdevice_type;
      hdr_active = config.dynamicRange > 0;
      const auto capture_framerate = normalize_framerate_hz(
        config.framerate > 0 ? static_cast<uint32_t>(config.framerate) : 60);
      delay = std::chrono::nanoseconds{1s} / capture_framerate;

      if (::video::is_encoder_probing_active) {
        // This is an encoder probe (boot-time or stream pre-flight).
        // We don't want to pop up a Portal dialog just to probe encoders.
        // Skip Portal init, set dummy resolution.
        width = config.width ? config.width : 1920;
        height = config.height ? config.height : 1080;
        env_width = width;
        env_height = height;
        BOOST_LOG(info) << "PipeWire: Skipping portal init for encoder probe (dummy mode)"sv;
        return 0;
      }

      int pipewire_fd = -1;
      uint32_t pipewire_node_id = PW_ID_ANY;

#ifdef SUNSHINE_BUILD_KWIN
      // Prefer KWin's direct screencast protocol for named Sonnenschein
      // virtual outputs. The KDE portal "Virtual Display" source is not
      // the same output and has been observed to negotiate 1920x1080.
      if (!display_name.empty()) {
        kwin_direct = std::make_unique<kwin_session_t>();
        const bool requested_hdr = config.dynamicRange > 0;
        if (kwin_direct->init() && kwin_direct->start(display_name, config.width, config.height, config.framerate, requested_hdr)) {
          pipewire_fd = -1;
          pipewire_node_id = kwin_direct->node_id;
          hdr_active = requested_hdr;
          if (requested_hdr && !kwin_direct->is_hdr_confirmed()) {
            BOOST_LOG(warning) << "KWin direct capture: client requested HDR; proceeding with HDR metadata even though KWin did not confirm HDR on the virtual output"sv;
          }
          BOOST_LOG(info) << "KWin direct capture: selected for output '"sv << display_name << "'"sv;
        } else {
          kwin_direct.reset();
          BOOST_LOG(warning)
            << "KWin direct capture: unavailable for output '"sv
            << display_name
            << "'. Falling back to xdg-desktop-portal monitor capture so the session can still start. "
            << "If KDE offers both a monitor and a virtual source, choose the Sonnenschein monitor output."sv;
        }
      }
#endif

      // Phase 1: Portal session fallback for non-KWin/non-named capture.
      if (pipewire_node_id == PW_ID_ANY) {
        portal = std::make_unique<portal_session_t>();
        if (!portal->init()) return -1;
        if (!portal->create_session()) return -1;
        if (!portal->select_sources()) return -1;
        if (!portal->start()) return -1;
        pipewire_fd = portal->pw_fd;
        pipewire_node_id = portal->pw_node_id;
      }

      // Phase 2: PipeWire stream
      pw_cap = std::make_unique<pw_capture_t>();
      if (!pw_cap->init(pipewire_fd, pipewire_node_id,
                        config.width > 0 ? static_cast<uint32_t>(config.width) : 1920,
                        config.height > 0 ? static_cast<uint32_t>(config.height) : 1080,
                        config.framerate > 0 ? static_cast<uint32_t>(config.framerate) : 60)) {
        return -1;
      }

      // Wait for format negotiation (up to 5s)
      for (int i = 0; i < 50 && pw_cap->width == 0; ++i) {
        std::this_thread::sleep_for(100ms);
      }

      if (pw_cap->width == 0) {
        BOOST_LOG(error) << "PipeWire: no frames received within 5s"sv;
        return -1;
      }

      width = pw_cap->width;
      height = pw_cap->height;
      env_width = width;
      env_height = height;

      BOOST_LOG(info) << "PipeWire display initialized: "sv << width << "x"sv << height;
      return 0;
    }

    bool is_hdr() override {
      return hdr_active;
    }

    bool get_hdr_metadata(SS_HDR_METADATA &metadata) override {
      if (!hdr_active) {
        std::memset(&metadata, 0, sizeof(metadata));
        return false;
      }

      std::memset(&metadata, 0, sizeof(metadata));
      // BT.2020 primaries and D65 white point, normalized to 50000.
      metadata.displayPrimaries[0].x = 35400; // R 0.708
      metadata.displayPrimaries[0].y = 14600; // R 0.292
      metadata.displayPrimaries[1].x = 8500;  // G 0.170
      metadata.displayPrimaries[1].y = 39850; // G 0.797
      metadata.displayPrimaries[2].x = 6550;  // B 0.131
      metadata.displayPrimaries[2].y = 2300;  // B 0.046
      metadata.whitePoint.x = 15635;          // D65 0.3127
      metadata.whitePoint.y = 16450;          // D65 0.3290
      metadata.maxDisplayLuminance = 1000;
      metadata.minDisplayLuminance = 1;
      metadata.maxContentLightLevel = 1000;
      metadata.maxFrameAverageLightLevel = 400;
      metadata.maxFullFrameLuminance = 400;
      return true;
    }

    platf::capture_e capture(
      const push_captured_image_cb_t &push_captured_image_cb,
      const pull_free_image_cb_t &pull_free_image_cb,
      bool *cursor
    ) override {
      if (!pw_cap) {
        std::this_thread::sleep_for(delay);
        std::shared_ptr<platf::img_t> img_out;
        if (!pull_free_image_cb(img_out)) return platf::capture_e::interrupted;
        if (!push_captured_image_cb(std::move(img_out), false)) return platf::capture_e::interrupted;
        return platf::capture_e::ok;
      }

      auto next_frame = std::chrono::steady_clock::now();
      uint64_t last_frame_sequence = 0;
      sleep_overshoot_logger.reset();

      while (true) {
        auto now = std::chrono::steady_clock::now();
        if (next_frame > now) {
          std::this_thread::sleep_for(next_frame - now);
          sleep_overshoot_logger.first_point(next_frame);
          sleep_overshoot_logger.second_point_now_and_log();
        }
        next_frame += delay;
        if (next_frame < now) next_frame = now + delay;

        // Keep the encoder paced at the client-requested rate. If KWin only
        // produces 60 new frames for a 90 Hz client, reuse the latest frame
        // instead of stalling the capture loop down to the compositor rate.
        uint64_t current_frame_sequence = last_frame_sequence;
        {
          std::unique_lock lk(pw_cap->frame_mtx);
          if (!pw_cap->has_frame) {
            pw_cap->frame_cv.wait_for(lk, std::chrono::milliseconds(500),
              [this]{ return pw_cap->has_frame; });
          } else if (pw_cap->frame_sequence == last_frame_sequence) {
            auto duplicate_wait = std::min<std::chrono::milliseconds>(
              std::chrono::duration_cast<std::chrono::milliseconds>(delay),
              2ms);
            if (duplicate_wait.count() <= 0) {
              duplicate_wait = 1ms;
            }
            pw_cap->frame_cv.wait_for(lk, duplicate_wait,
              [this, last_frame_sequence]{ return pw_cap->frame_sequence != last_frame_sequence; });
          }

          if (!pw_cap->has_frame) {
            std::shared_ptr<platf::img_t> img_out;
            if (!push_captured_image_cb(std::move(img_out), false)) {
              return platf::capture_e::ok;
            }
            continue;
          }

          current_frame_sequence = pw_cap->frame_sequence;
          pw_cap->has_new_frame = false;

          // Check for resolution change
          if (pw_cap->width != (uint32_t)width || pw_cap->height != (uint32_t)height) {
            width = pw_cap->width;
            height = pw_cap->height;
            return platf::capture_e::reinit;
          }
        }

        std::shared_ptr<platf::img_t> img_out;
        if (!pull_free_image_cb(img_out)) {
          return platf::capture_e::interrupted;
        }

        auto frame_ts = std::chrono::steady_clock::now();

        // Copy SHM frame data to image
        if (!pw_cap->is_dmabuf) {
          std::lock_guard lk(pw_cap->frame_mtx);
          auto row_bytes = std::min((int)pw_cap->stride, img_out->row_pitch);
          auto rows = std::min((int)pw_cap->height, img_out->height);
          for (int y = 0; y < rows; ++y) {
            std::memcpy(
              img_out->data + y * img_out->row_pitch,
              pw_cap->frame_data.data() + y * pw_cap->stride,
              row_bytes);
          }
        }

        img_out->frame_timestamp = frame_ts;
        last_frame_sequence = current_frame_sequence;

        if (!push_captured_image_cb(std::move(img_out), true)) {
          return platf::capture_e::ok;
        }
      }
      return platf::capture_e::ok;
    }

    std::shared_ptr<platf::img_t> alloc_img() override {
      auto img = std::make_shared<pw::img_t>();
      img->width = width;
      img->height = height;
      img->pixel_pitch = 4;
      img->row_pitch = img->pixel_pitch * width;
      img->data = new std::uint8_t[height * img->row_pitch]();
      return img;
    }

    int dummy_img(platf::img_t *img) override {
      return 0;
    }

    std::unique_ptr<platf::avcodec_encode_device_t> make_avcodec_encode_device(platf::pix_fmt_e pix_fmt) override {
#ifdef SUNSHINE_BUILD_VAAPI
      if (mem_type == platf::mem_type_e::vaapi) {
        return va::make_avcodec_encode_device(width, height, false);
      }
#endif
      return std::make_unique<platf::avcodec_encode_device_t>();
    }

    platf::mem_type_e mem_type;
    std::chrono::nanoseconds delay;
    bool hdr_active = false;
#ifdef SUNSHINE_BUILD_KWIN
    std::unique_ptr<kwin_session_t> kwin_direct;
#endif
    std::unique_ptr<portal_session_t> portal;
    std::unique_ptr<pw_capture_t> pw_cap;
  };

}  // namespace pw

namespace platf {
  std::shared_ptr<display_t> pw_display(mem_type_e hwdevice_type, const std::string &display_name, const video::config_t &config) {
    // PipeWire always delivers system memory (SHM) frames. Encoders like NVENC request CUDA,
    // but the Sunshine encoder framework will automatically fallback to GPU->RAM->GPU copy
    // if the display provides system memory.
    auto effective_type = (hwdevice_type == platf::mem_type_e::cuda) ? platf::mem_type_e::system : hwdevice_type;

    if (effective_type != platf::mem_type_e::system && effective_type != platf::mem_type_e::vaapi) {
      BOOST_LOG(error) << "PipeWire: unsupported hw device type"sv;
      return nullptr;
    }

    auto disp = std::make_shared<pw::pw_display_t>();
    if (disp->init(effective_type, display_name, config)) {
      return nullptr;
    }
    return disp;
  }
  std::vector<std::string> pw_display_names() {
    // PipeWire portal doesn't enumerate — returns a single virtual entry
    return {"pipewire-virtual"};
  }
}  // namespace platf

#endif  // SUNSHINE_BUILD_PIPEWIRE
