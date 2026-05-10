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

        // 1. Add the virtual output. Plasma 6.4+ syntax:
        //    kscreen-doctor add-virtual-output NAME WIDTH HEIGHT [SCALE]
        const auto add = run_args(
            {"kscreen-doctor",
             "add-virtual-output",
             disp,
             std::to_string(req.width),
             std::to_string(req.height)},
            "kscreen-doctor add-virtual-output");

        if (!add.ok) {
          // The error is most often:
          //   - "Unknown command: add-virtual-output"
          //     → Plasma < 6.4. Phase 2B.5 (KWin plugin) needed.
          //   - "Could not connect to KWin"
          //     → Sonnenschein not running in the user's KDE session;
          //       check that the systemd unit is --user, not system-mode.
          //   - "Output already exists"
          //     → A previous Sonnenschein session leaked. We try once more
          //       after issuing remove-virtual-output to recover.
          if (add.output.find("already exists") != std::string::npos) {
            BOOST_LOG(warning)
                << "Sonnenschein vdisplay (kwin): output '" << disp
                << "' already exists, removing and retrying.";
            auto cleanup = run_args({"kscreen-doctor", "remove-virtual-output", disp},
                     "kscreen-doctor remove-virtual-output (cleanup)");
            (void)cleanup;
            const auto retry = run_args(
                {"kscreen-doctor",
                 "add-virtual-output",
                 disp,
                 std::to_string(req.width),
                 std::to_string(req.height)},
                "kscreen-doctor add-virtual-output (retry)");
            if (!retry.ok) {
              BOOST_LOG(error)
                  << "Sonnenschein vdisplay (kwin): retry add failed: "
                  << retry.output;
              return std::nullopt;
            }
          } else {
            BOOST_LOG(error)
                << "Sonnenschein vdisplay (kwin): add-virtual-output failed: "
                << add.output;
            return std::nullopt;
          }
        } else {
          BOOST_LOG(info) << "Sonnenschein vdisplay (kwin): kscreen-doctor add-virtual-output returned: " << add.output;
        }

        // 2. Best-effort: set the exact mode, including refresh rate.
        //    Plasma's mode-id format: "WxH@HZ".
        if (req.refresh_mhz > 0) {
          std::ostringstream mode_arg;
          mode_arg << "output." << disp << ".mode."
                   << req.width << "x" << req.height
                   << "@" << format_refresh_hz(req.refresh_mhz);
          const auto mode = run_args(
              {"kscreen-doctor", mode_arg.str()},
              "kscreen-doctor mode set");
          if (!mode.ok) {
            BOOST_LOG(warning)
                << "Sonnenschein vdisplay (kwin): mode set failed for '"
                << disp << "' ("
                << req.width << "x" << req.height << "@"
                << format_refresh_hz(req.refresh_mhz)
                << " Hz). KWin will use a default mode. Output: "
                << mode.output;
          }
        }

        // 3. HDR if the client is HDR-capable. KWin 6.2+ honors per-output
        //    HDR enable; on NVIDIA this requires Driver ≥ 580.94.11 (Wayland
        //    only). Plain failure is non-fatal — we fall back to SDR.
        bool hdr_active = false;
        if (req.hdr_capable) {
          const auto hdr = run_args(
              {"kscreen-doctor", "output." + disp + ".hdr.true"},
              "kscreen-doctor hdr enable");
          if (hdr.ok) {
            hdr_active = true;
          } else {
            BOOST_LOG(warning)
                << "Sonnenschein vdisplay (kwin): HDR enable failed for '"
                << disp << "'; falling back to SDR. Output: " << hdr.output;
          }
        }

        Handle h;
        h.display_id = disp;
        h.output_name = disp;
        h.width = req.width;
        h.height = req.height;
        h.refresh_mhz = req.refresh_mhz;
        h.hdr_active = hdr_active;
        // gpu_pci_id stays empty — KWin picks the rendering GPU itself
        // based on its primary-render selection. Phase 2 doesn't expose
        // GPU pinning yet; Phase 5 (WebUI) wires that in.

        {
          std::lock_guard<std::mutex> lock(mu_);
          active_[disp] = ActiveDisplay{
              req.client_uid,
              req.width,
              req.height,
              req.refresh_mhz,
              hdr_active,
          };
        }

        std::string hz_str = format_refresh_hz(req.refresh_mhz);
        BOOST_LOG(info)
            << "Sonnenschein vdisplay (kwin): created '" << disp << "' "
            << req.width << "x" << req.height
            << "@" << hz_str.c_str() << " Hz"
            << (hdr_active ? " HDR10" : " SDR");
        return h;
      }

      void destroy(const std::string& display_id) override {
        if (display_id.empty()) {
          return;
        }
        {
          std::lock_guard<std::mutex> lock(mu_);
          if (active_.erase(display_id) == 0) {
            // Not ours — be defensive and don't poke an unrelated output.
            return;
          }
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
      };

      mutable std::mutex mu_;
      std::unordered_map<std::string, ActiveDisplay> active_;
    };

  }  // namespace

  std::unique_ptr<IBackend> make_kwin_wayland() {
    return std::make_unique<KwinWaylandBackend>();
  }

}  // namespace sonnenschein::vdisplay::backends
