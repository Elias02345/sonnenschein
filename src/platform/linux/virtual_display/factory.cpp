/**
 * @file src/platform/linux/virtual_display/factory.cpp
 * @brief Selects the right IBackend for the running environment.
 */
#include "factory.h"

#include "backends/all.h"
#include "detection.h"
#include "src/logging.h"

#include <algorithm>
#include <array>
#include <utility>

namespace sonnenschein::vdisplay {
  namespace {

    /// Build the priority-ordered list of all known backend factory functions.
    /// Order matters — first available() == true wins under "auto".
    /// Rationale for the order: prefer in-session compositor integrations
    /// (KWin / Mutter / wlroots) over Xorg approaches over evdi-as-last-resort.
    using FactoryFn = std::unique_ptr<IBackend> (*)();

    constexpr std::array<FactoryFn, 7> kFactories = {
        &backends::make_kwin_wayland,
        &backends::make_mutter_headless,
        &backends::make_wlroots_headless,
        &backends::make_xorg_nvidia,
        &backends::make_xorg_dummy,
        &backends::make_amdgpu_param,
        &backends::make_evdi,
    };

  }  // namespace

  std::vector<std::unique_ptr<IBackend>> all_backends() {
    std::vector<std::unique_ptr<IBackend>> out;
    out.reserve(kFactories.size());
    for (auto fn : kFactories) {
      out.emplace_back(fn());
    }
    return out;
  }

  std::unique_ptr<IBackend> select_backend(const Environment& env, const Config& cfg) {
    // Manual override path.
    if (!cfg.preferred_backend.empty() && cfg.preferred_backend != "auto") {
      for (auto fn : kFactories) {
        auto candidate = fn();
        if (!candidate) {
          continue;
        }
        if (candidate->name() == cfg.preferred_backend) {
          if (!candidate->available(env)) {
            BOOST_LOG(warning)
                << "Sonnenschein: user-selected virtual-display backend '"
                << cfg.preferred_backend
                << "' reports !available on this system (compositor="
                << to_string(env.compositor) << ", display_server="
                << to_string(env.display_server)
                << "). Trying it anyway — set virtual_display.backend=auto in "
                   "sonnenschein.conf to fall back to heuristic selection.";
          }
          return candidate;
        }
      }
      BOOST_LOG(warning)
          << "Sonnenschein: unknown virtual-display backend '"
          << cfg.preferred_backend
          << "'. Falling back to auto-selection. Known backends: "
             "kwin_wayland, mutter_headless, wlroots_headless, xorg_nvidia, "
             "xorg_dummy, amdgpu_param, evdi.";
      // fall through to auto
    }

    // Auto path — first available wins.
    for (auto fn : kFactories) {
      auto candidate = fn();
      if (!candidate) {
        continue;
      }
      if (candidate->available(env)) {
        BOOST_LOG(info)
            << "Sonnenschein: auto-selected virtual-display backend '"
            << candidate->name() << "' (" << candidate->display_name()
            << ") for compositor=" << to_string(env.compositor);
        return candidate;
      }
    }

    BOOST_LOG(warning)
        << "Sonnenschein: no virtual-display backend matched the running "
           "environment (compositor=" << to_string(env.compositor)
        << ", display_server=" << to_string(env.display_server)
        << ", gpus=" << env.gpus.size()
        << "). Falling back to streaming the existing primary display.";
    return nullptr;
  }

}  // namespace sonnenschein::vdisplay
