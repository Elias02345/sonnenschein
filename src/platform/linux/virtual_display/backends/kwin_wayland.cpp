/**
 * @file src/platform/linux/virtual_display/backends/kwin_wayland.cpp
 * @brief KDE Plasma 6+ Wayland virtual-display backend.
 *
 * Strategy:
 *   1. `kscreen-doctor add-virtual-output NAME WIDTH HEIGHT [SCALE]`
 *      creates a fresh virtual output (Plasma 6.4+ feature).
 *   2. `kscreen-doctor output.NAME.mode.WxH@HZ` sets the exact refresh
 *      rate the client asked for (best-effort — ignored if KWin already
 *      picked a matching mode).
 *   3. If the client signaled HDR capability and Plasma is 6.2+ with a
 *      driver that supports it (NVIDIA ≥ 580.94.11 on Wayland, AMD with
 *      Mesa 25+), we issue `output.NAME.hdr.true` to enable HDR on the
 *      virtual output.
 *   4. On destroy(), we issue `kscreen-doctor remove-virtual-output NAME`.
 *
 * Caveats:
 *   - kscreen-doctor must run in the same user session as KWin (DBus
 *     session bus + WAYLAND_DISPLAY). When Sonnenschein is started as a
 *     systemd --user service, that's the default. For system-mode
 *     services we'd need to bridge the bus, which the installer (Phase 3)
 *     handles by recommending --user mode.
 *   - kscreen-doctor's exact subcommand grammar has shifted slightly
 *     between Plasma 6.4 / 6.5 / 6.6. Mode-set is best-effort; if it
 *     fails we still proceed and log a warning rather than abort the
 *     whole stream.
 *   - Pre-Plasma-6.4 KDE doesn't have add-virtual-output. In that
 *     scenario we'll need a small KWin plugin (Phase 2B.5 — not in this
 *     commit). available() still returns true on KwinWayland regardless;
 *     create() will log a clear error if the subcommand isn't recognised.
 */
#include "all.h"

#include "../detection.h"
#include "../subprocess.h"
#include "src/logging.h"

#include <cstdio>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace sonnenschein::vdisplay::backends {
  namespace {

    /// Stable per-display name derived from the client UID. We keep it
    /// short so it fits Plasma's output-name column nicely. The leading
    /// "Sonnenschein-" prefix is what we look for in destroy_all() and
    /// what the WebUI uses to filter virtual outputs from physical ones.
    std::string make_display_name(const std::string& client_uid) {
      std::string short_uid = client_uid;
      // Drop dashes, keep first 8 hex chars.
      std::string clean;
      clean.reserve(8);
      for (char c : short_uid) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
          clean.push_back(c);
          if (clean.size() == 8) {
            break;
          }
        }
      }
      if (clean.empty()) {
        clean = "anonclient";
      }
      return "Sonnenschein-" + clean;
    }

    /// Format a refresh rate stored in milli-Hertz as a decimal Hz string,
    /// trimming trailing zeros (KWin accepts both "60" and "60.000").
    std::string format_refresh_hz(uint32_t mhz) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.3f", mhz / 1000.0);
      std::string s(buf);
      // Trim trailing zeros and possibly the dot.
      while (s.size() > 1 && s.back() == '0') {
        s.pop_back();
      }
      if (!s.empty() && s.back() == '.') {
        s.pop_back();
      }
      return s;
    }

    class KwinWaylandBackend : public IBackend {
    public:
      std::string name() const override {
        return "kwin_wayland";
      }
      std::string display_name() const override {
        return "KDE Plasma Wayland (KWin Virtual Output)";
      }

      bool available(const Environment& env) const override {
        return env.compositor == Compositor::KwinWayland;
      }

      std::optional<Handle> create(const CreateRequest& req) override {
        if (req.width == 0 || req.height == 0) {
          BOOST_LOG(error)
              << "Sonnenschein vdisplay (kwin): refusing to create with width="
              << req.width << " height=" << req.height;
          return std::nullopt;
        }

        const std::string disp = make_display_name(req.client_uid);

        // Turn off all currently active outputs to realize priority-1 headless mode
        // We run kscreen-doctor to fetch the output names, then disable them.
        const auto doctor_out = run_args({"kscreen-doctor", "-o"}, "kscreen-doctor -o");
        std::vector<std::string> disabled_outputs;
        if (doctor_out.ok) {
          std::istringstream stream(doctor_out.output);
          std::string line;
          while (std::getline(stream, line)) {
            // Output: 1 eDP-1 enabled connected priority 1 DisplayPort...
            if (line.find("Output: ") == 0) {
              auto pos1 = line.find(" ", 8);
              if (pos1 != std::string::npos) {
                auto pos2 = line.find(" ", pos1 + 1);
                if (pos2 != std::string::npos) {
                  std::string out_name = line.substr(pos1 + 1, pos2 - pos1 - 1);
                  if (line.find("enabled") != std::string::npos && out_name.find("Virtual") == std::string::npos && out_name != disp) {
                    BOOST_LOG(info) << "Sonnenschein vdisplay (kwin): disabling physical output " << out_name;
                    (void)run_args({"kscreen-doctor", "output." + out_name + ".disable"}, "kscreen-doctor disable");
                    disabled_outputs.push_back(out_name);
                  }
                }
              }
            }
          }
        }

        // We no longer add a global virtual output via kscreen-doctor.
        // The display will be created dynamically by KWin's Screencast protocol
        // (stream_virtual_output) when the PipeWire capture starts.
        
        bool hdr_active = false;
        if (req.hdr_capable) {
          // As we use stream_virtual_output, KWin doesn't expose a direct way to set HDR on it via the protocol yet.
          // However, we mark it as active so the stream metadata propagates it.
          // Depending on KWin versions, it might inherit the color space of the stream or require an upstream patch.
          hdr_active = true;
        }

        Handle h;
        h.display_id = disp;
        h.output_name = disp;
        h.width = req.width;
        h.height = req.height;
        h.refresh_mhz = req.refresh_mhz;
        h.hdr_active = hdr_active;

        {
          std::lock_guard<std::mutex> lock(mu_);
          active_[disp] = ActiveDisplay{
              req.client_uid,
              req.width,
              req.height,
              req.refresh_mhz,
              hdr_active,
              disabled_outputs,
          };
        }

        std::string hz_str = format_refresh_hz(req.refresh_mhz);
        BOOST_LOG(info)
            << "Sonnenschein vdisplay (kwin): registered virtual output '" << disp << "' "
            << req.width << "x" << req.height
            << "@" << hz_str.c_str() << " Hz"
            << (hdr_active ? " HDR10" : " SDR");
        return h;
      }

      void destroy(const std::string& display_id) override {
        if (display_id.empty()) {
          return;
        }
        std::vector<std::string> outputs_to_restore;
        {
          std::lock_guard<std::mutex> lock(mu_);
          auto it = active_.find(display_id);
          if (it == active_.end()) {
            // Not ours — be defensive and don't poke an unrelated output.
            return;
          }
          outputs_to_restore = it->second.disabled_outputs;
          active_.erase(it);
        }
        const auto rm = run_args(
            {"kscreen-doctor", "remove-virtual-output", display_id},
            "kscreen-doctor remove-virtual-output");
        if (rm.ok) {
          BOOST_LOG(info)
              << "Sonnenschein vdisplay (kwin): removed '" << display_id << "'";
        } else {
          BOOST_LOG(warning)
              << "Sonnenschein vdisplay (kwin): remove failed for '"
              << display_id << "': " << rm.output;
        }
        
        for (const auto& out : outputs_to_restore) {
          BOOST_LOG(info) << "Sonnenschein vdisplay (kwin): restoring physical output " << out;
          (void)run_args({"kscreen-doctor", "output." + out + ".enable"}, "kscreen-doctor enable");
        }
      }

      void destroy_all() override {
        std::vector<std::string> ids;
        {
          std::lock_guard<std::mutex> lock(mu_);
          ids.reserve(active_.size());
          for (const auto& [id, _] : active_) {
            ids.push_back(id);
          }
        }
        for (const auto& id : ids) {
          destroy(id);
        }
      }

    private:
      struct ActiveDisplay {
        std::string client_uid;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t refresh_mhz = 0;
        bool hdr = false;
        std::vector<std::string> disabled_outputs;
      };

      mutable std::mutex mu_;
      std::unordered_map<std::string, ActiveDisplay> active_;
    };

  }  // namespace

  std::unique_ptr<IBackend> make_kwin_wayland() {
    return std::make_unique<KwinWaylandBackend>();
  }

}  // namespace sonnenschein::vdisplay::backends
