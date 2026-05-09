/**
 * @file src/platform/linux/virtual_display/factory.h
 * @brief Backend selection — picks the right IBackend for the current
 * environment, respecting user overrides from sonnenschein.conf.
 */
#pragma once

#include "interface.h"
#include "types.h"

#include <memory>
#include <string>
#include <vector>

namespace sonnenschein::vdisplay {

  /// User-controllable selection knobs. Loaded from sonnenschein.conf
  /// (Phase 5 hooks WebUI to this).
  struct Config {
    /// "" or "auto" → use heuristic. Otherwise must match an IBackend::name(),
    /// e.g. "kwin_wayland", "xorg_nvidia", "mutter_headless", "wlroots_headless",
    /// "xorg_dummy", "amdgpu_param", "evdi".
    std::string preferred_backend;

    /// "" or "auto" → use Environment::primary_gpu_idx. Otherwise a PCI
    /// address like "0000:01:00.0" — must match a known GPU.
    std::string preferred_gpu_pci;
  };

  /// Construct the backend that best matches the current environment.
  ///
  /// Selection rules (in order):
  ///   1. If cfg.preferred_backend is set and not "auto": load that exact
  ///      backend, log a warning if it reports !available(env).
  ///   2. Else: walk the candidate list in priority order, return the first
  ///      one whose available(env) returns true.
  ///   3. If nothing matches: return nullptr — caller should log and fall
  ///      back to streaming the existing primary display (no virtual output).
  [[nodiscard]] std::unique_ptr<IBackend> select_backend(
      const Environment& env,
      const Config& cfg
  );

  /// All backend implementations Sonnenschein knows about, in the order
  /// the factory considers them. Useful for the WebUI dropdown ("which
  /// backends *could* run?") and for diagnostic logging.
  ///
  /// Each call constructs fresh instances — they're cheap stubs until the
  /// factory has chosen one.
  [[nodiscard]] std::vector<std::unique_ptr<IBackend>> all_backends();

}  // namespace sonnenschein::vdisplay
