/**
 * @file src/platform/linux/virtual_display/interface.h
 * @brief Sonnenschein virtual-display backend interface.
 *
 * One IBackend per (compositor, GPU vendor) combo. Selected at runtime by
 * factory::select_backend(). Backends are responsible for creating a
 * display matching the client's request, returning a Handle, and tearing
 * the display down when destroy() is called.
 *
 * Backends live in src/platform/linux/virtual_display/backends/.
 */
#pragma once

#include "types.h"

#include <memory>
#include <optional>
#include <string>

namespace sonnenschein::vdisplay {

  class IBackend {
  public:
    virtual ~IBackend() = default;

    /// Identifier, e.g. "kwin_wayland", "xorg_nvidia". Stable string used
    /// in config files and logs.
    [[nodiscard]] virtual std::string name() const = 0;

    /// Human-readable label for the WebUI dropdown,
    /// e.g. "KDE Plasma Wayland (KWin Virtual Output)".
    [[nodiscard]] virtual std::string display_name() const = 0;

    /// Quick check that this backend's preconditions are satisfied on the
    /// running system. Called both during factory selection (to skip
    /// non-applicable backends) and when the user manually selects one
    /// (to surface a friendly error in the WebUI).
    ///
    /// Implementations must be cheap — avoid spawning processes here.
    /// Use detection::Environment for the heavy lifting.
    [[nodiscard]] virtual bool available(const Environment&) const = 0;

    /// Create a virtual display.
    ///
    /// @returns populated Handle on success; empty optional on failure.
    /// On failure, the implementation should have logged the reason via
    /// BOOST_LOG so the user can see it in journalctl.
    [[nodiscard]] virtual std::optional<Handle> create(const CreateRequest&) = 0;

    /// Tear the virtual display down. Idempotent — safe to call with an
    /// unknown display_id (no-op).
    virtual void destroy(const std::string& display_id) = 0;

    /// Tear all displays this backend created down at once. Called on
    /// process shutdown so we don't leave orphaned virtual outputs.
    virtual void destroy_all() = 0;
  };

}  // namespace sonnenschein::vdisplay
