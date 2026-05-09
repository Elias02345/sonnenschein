/**
 * @file src/platform/linux/virtual_display/backends/kwin_wayland.cpp
 * @brief KDE Plasma 6+ Wayland backend (stub — Phase 2B implements).
 *
 * Plan for the real implementation:
 *   - Plasma 6.4+ has `kscreen-doctor add-virtual-output WIDTHxHEIGHT@HZ NAME`.
 *     That's our happy path: shell out, parse the output name, call
 *     `kscreen-doctor output.NAME.priority.1` to make it primary, and
 *     `output.NAME.disable` for siblings if isolated mode is enabled.
 *   - Plasma 6.0–6.3: kscreen-doctor lacks add-virtual-output. Fallback to
 *     a tiny KWin plugin that registers a virtual output via the internal
 *     `KWin::OutputBackend::createVirtualOutput` API. We ship the plugin
 *     in packaging/linux/kwin-plugin/ and the installer drops it into
 *     /usr/lib/kwin/plugins/.
 *   - HDR: KWin honors per-output HDR enable (Plasma 6.2+), so we issue
 *     `kscreen-doctor output.NAME.hdr.true` when launch_session signals
 *     dynamicRangeMode > 0.
 *   - Capture: existing kwingrab.cpp / portalgrab.cpp paths capture
 *     whichever output is active — once the virtual output is the primary,
 *     capture targets it automatically.
 */
#include "all.h"
#include "../detection.h"

namespace sonnenschein::vdisplay::backends {
  namespace {

    class KwinWaylandStub : public IBackend {
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
      std::optional<Handle> create(const CreateRequest&) override {
        return std::nullopt;  // Phase 2B implements.
      }
      void destroy(const std::string&) override {}
      void destroy_all() override {}
    };

  }  // namespace

  std::unique_ptr<IBackend> make_kwin_wayland() {
    return std::make_unique<KwinWaylandStub>();
  }

}  // namespace sonnenschein::vdisplay::backends
