/**
 * @file src/platform/linux/virtual_display/backends/kwin_wayland.cpp
 * @brief KDE Plasma 6+ Wayland virtual-display backend.
 *
 * Strategy (post-2026-05-14 hybrid):
 *   1. Disable all currently-enabled physical outputs via kscreen-doctor
 *      so the streamed virtual display becomes the only active output
 *      (priority-1 headless mode).
 *   2. **Pre-create** the virtual output via
 *      `kscreen-doctor add-virtual-output NAME WIDTH HEIGHT` so KWin
 *      exposes it through the Wayland registry with a sensible mode
 *      list **before** the capture stream is opened. This is the only
 *      reliable way to land at the client's requested refresh rate —
 *      `zkde_screencast_unstable_v1::stream_virtual_output` has no
 *      refresh-rate argument and pins newly-created outputs at 60 Hz,
 *      which then refuses any later `mode set` because the requested
 *      mode isn't in the output's mode list.
 *   3. `kscreen-doctor output.NAME.mode.WxH@HZ` selects the exact mode
 *      the client asked for (e.g. 1280x800@90 from a Steam Deck OLED).
 *      Best-effort — if it fails KWin will pick a default mode and we
 *      keep streaming rather than abort.
 *   4. If the client signaled HDR capability (Plasma 6.2+, NVIDIA driver
 *      ≥ 580.94.11 on Wayland, or AMD with Mesa 25+), `output.NAME.hdr.true`
 *      enables HDR on the virtual output. Best-effort.
 *   5. pwgrab.cpp's find_target() then locates the pre-created output in
 *      the Wayland registry and uses `stream_output` (with the wl_output
 *      we just configured) rather than `stream_virtual_output`. Refresh
 *      rate comes through the existing mode.
 *   6. If add-virtual-output fails (Plasma < 6.4 or kscreen-doctor not
 *      reachable), we fall through: pwgrab.cpp will fail to find the
 *      output, fall back to `stream_virtual_output`, and stream at
 *      60 Hz default. That's the previous behaviour — degraded but
 *      not broken.
 *   7. On destroy(): kscreen-doctor remove-virtual-output NAME (only if
 *      we successfully added it), then re-enable the physical outputs
 *      we disabled.
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
        const std::string hz_str = format_refresh_hz(req.refresh_mhz);

        // --- Step 1: Disable all physical outputs (priority-1 headless mode). ---
        // We query kscreen-doctor for the active outputs, then disable each
        // one (except the virtual one we're about to create). On destroy()
        // they get re-enabled.
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

        // --- Step 2: Pre-create the virtual output via kscreen-doctor. ---
        //
        // This is the key change vs. the 60-Hz-stuck flow. zkde_screencast's
        // stream_virtual_output() takes no refresh-rate argument and pins
        // newly-created outputs at 60 Hz, after which kscreen-doctor mode
        // set is rejected (the requested mode isn't in the output's mode
        // list yet). Pre-creating via kscreen-doctor populates the mode list
        // with everything KWin supports for the given WxH, which then lets
        // mode-set in step 3 select the right refresh rate.
        bool added_via_kscreen = false;
        const auto add = run_args(
            {"kscreen-doctor", "add-virtual-output", disp,
             std::to_string(req.width), std::to_string(req.height)},
            "kscreen-doctor add-virtual-output");
        if (add.ok) {
          added_via_kscreen = true;
          BOOST_LOG(info)
              << "Sonnenschein vdisplay (kwin): pre-created virtual output '"
              << disp << "' " << req.width << "x" << req.height;
        } else if (add.output.find("already exists") != std::string::npos) {
          // Leftover from a previous session — clean up and retry once.
          BOOST_LOG(warning)
              << "Sonnenschein vdisplay (kwin): output '" << disp
              << "' already exists, removing and retrying.";
          (void)run_args({"kscreen-doctor", "remove-virtual-output", disp},
                         "kscreen-doctor remove-virtual-output (cleanup)");
          const auto retry = run_args(
              {"kscreen-doctor", "add-virtual-output", disp,
               std::to_string(req.width), std::to_string(req.height)},
              "kscreen-doctor add-virtual-output (retry)");
          if (retry.ok) {
            added_via_kscreen = true;
            BOOST_LOG(info)
                << "Sonnenschein vdisplay (kwin): pre-created virtual output '"
                << disp << "' after retry";
          } else {
            BOOST_LOG(warning)
                << "Sonnenschein vdisplay (kwin): retry add failed, falling "
                << "back to stream_virtual_output (60Hz-locked): "
                << retry.output;
          }
        } else {
          // Common failures:
          //   - "Unknown command: add-virtual-output" → Plasma < 6.4
          //   - "Could not connect to KWin" → wrong DBus session
          // Non-fatal: pwgrab.cpp will fall back to stream_virtual_output.
          BOOST_LOG(warning)
              << "Sonnenschein vdisplay (kwin): add-virtual-output failed, "
              << "falling back to stream_virtual_output (will stream at 60Hz "
              << "default). Output: " << add.output;
        }

        // --- Step 3: Set the exact mode (width x height @ refresh) on the
        //     pre-created output. Only meaningful if step 2 succeeded.
        if (added_via_kscreen && req.refresh_mhz > 0) {
          std::ostringstream mode_arg;
          // The literal '@' character was historically corrupted by the
          // compiler/macro chain in some builds; using its hex code keeps
          // the string clean. Format expected by Plasma's CLI:
          //     output.<NAME>.mode.<WxH>@<HZ>
          const char at_sign = 0x40;
          mode_arg << "output." << disp << ".mode."
                   << req.width << "x" << req.height
                   << at_sign << hz_str;
          BOOST_LOG(info)
              << "Sonnenschein vdisplay (kwin): setting mode via "
              << mode_arg.str();
          const auto mode = run_args(
              {"kscreen-doctor", mode_arg.str()},
              "kscreen-doctor mode set");
          if (!mode.ok) {
            BOOST_LOG(warning)
                << "Sonnenschein vdisplay (kwin): mode set failed for '"
                << disp << "' (" << req.width << "x" << req.height
                << "@" << hz_str << " Hz). KWin will keep its default mode. "
                << "Output: " << mode.output;
          }
        }

        // --- Step 4: HDR (Plasma 6.2+, best-effort). ---
        bool hdr_active = false;
        if (added_via_kscreen && req.hdr_capable) {
          const auto hdr = run_args(
              {"kscreen-doctor", "output." + disp + ".hdr.true"},
              "kscreen-doctor hdr enable");
          if (hdr.ok) {
            hdr_active = true;
            BOOST_LOG(info)
                << "Sonnenschein vdisplay (kwin): HDR enabled on '" << disp << "'";
          } else {
            BOOST_LOG(warning)
                << "Sonnenschein vdisplay (kwin): HDR enable failed for '"
                << disp << "'; falling back to SDR. Output: " << hdr.output;
          }
        } else if (!added_via_kscreen && req.hdr_capable) {
          // Without a pre-created output we have no surface to flip HDR on.
          // We still set hdr_active so the encoder negotiates 10-bit metadata
          // for the stream itself; KWin may or may not honour it.
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
              added_via_kscreen,
              disabled_outputs,
          };
        }

        BOOST_LOG(info)
            << "Sonnenschein vdisplay (kwin): "
            << (added_via_kscreen ? "registered" : "scheduled (lazy)")
            << " virtual output '" << disp << "' "
            << req.width << "x" << req.height
            << "@" << hz_str << " Hz"
            << (hdr_active ? " HDR10" : " SDR");
        return h;
      }

      void destroy(const std::string& display_id) override {
        if (display_id.empty()) {
          return;
        }
        std::vector<std::string> outputs_to_restore;
        bool was_added_via_kscreen = false;
        {
          std::lock_guard<std::mutex> lock(mu_);
          auto it = active_.find(display_id);
          if (it == active_.end()) {
            // Not ours — be defensive and don't poke an unrelated output.
            return;
          }
          outputs_to_restore = it->second.disabled_outputs;
          was_added_via_kscreen = it->second.added_via_kscreen;
          active_.erase(it);
        }
        // Only issue remove-virtual-output when we actually added it via
        // kscreen-doctor. If KWin's stream_virtual_output created it
        // implicitly (Plasma < 6.4 fallback), it tears itself down when
        // pwgrab.cpp closes the stream.
        if (was_added_via_kscreen) {
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
        // True iff we successfully called `kscreen-doctor add-virtual-output`
        // for this display. destroy() uses this to decide whether to call
        // remove-virtual-output (only ours to remove).
        bool added_via_kscreen = false;
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
