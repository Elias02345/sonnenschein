/**
 * @file src/platform/linux/virtual_display/backends/wlroots_headless.cpp
 * @brief Sway / Hyprland (wlroots-derived) backend (stub).
 *
 * Plan:
 *   - Sway: `swaymsg create_output HEADLESS-1` + `output HEADLESS-1 mode WxH@HZ`.
 *     Stable, well-documented, our reference wlroots target.
 *   - Hyprland: `hyprctl output create headless` (added in 0.42, Sonnenschein
 *     requires a recent build). API drifts; best-effort.
 *   - WLR_BACKENDS=headless,libinput when we spawn the compositor ourselves
 *     (headless boot scenario).
 */
#include "all.h"
#include "../detection.h"

namespace sonnenschein::vdisplay::backends {
  namespace {

    class WlrootsHeadlessStub : public IBackend {
    public:
      std::string name() const override {
        return "wlroots_headless";
      }
      std::string display_name() const override {
        return "wlroots compositor (Sway / Hyprland)";
      }
      bool available(const Environment& env) const override {
        return env.compositor == Compositor::Sway || env.compositor == Compositor::Hyprland;
      }
      std::optional<Handle> create(const CreateRequest&) override {
        return std::nullopt;
      }
      void destroy(const std::string&) override {}
      void destroy_all() override {}
    };

  }  // namespace

  std::unique_ptr<IBackend> make_wlroots_headless() {
    return std::make_unique<WlrootsHeadlessStub>();
  }

}  // namespace sonnenschein::vdisplay::backends
