/**
 * @file src/platform/linux/virtual_display/backends/mutter_headless.cpp
 * @brief GNOME / Mutter Wayland headless backend (stub).
 *
 * Plan:
 *   - Two flavors, depending on whether Sonnenschein is the desktop session
 *     or running alongside one:
 *       a) Headless boot: spawn `mutter --wayland --headless --virtual-monitor
 *          WxH@HZ` ourselves and treat its output as the streaming target.
 *          systemd unit owns lifecycle.
 *       b) Active GNOME session: call into the running Mutter via the
 *          org.gnome.Mutter.RemoteDesktop / ScreenCast D-Bus interfaces,
 *          which can spawn a virtual monitor inside the live session.
 *   - HDR: Mutter color-management protocol is partial as of 47.x; we
 *     fall back to SDR if the compositor doesn't advertise HDR.
 *   - Capture: PipeWire portal, already wired in Apollo's portalgrab.cpp.
 */
#include "all.h"
#include "../detection.h"

namespace sonnenschein::vdisplay::backends {
  namespace {

    class MutterHeadlessStub : public IBackend {
    public:
      std::string name() const override {
        return "mutter_headless";
      }
      std::string display_name() const override {
        return "GNOME (Mutter headless / virtual-monitor)";
      }
      bool available(const Environment& env) const override {
        return env.compositor == Compositor::Mutter;
      }
      std::optional<Handle> create(const CreateRequest&) override {
        return std::nullopt;
      }
      void destroy(const std::string&) override {}
      void destroy_all() override {}
    };

  }  // namespace

  std::unique_ptr<IBackend> make_mutter_headless() {
    return std::make_unique<MutterHeadlessStub>();
  }

}  // namespace sonnenschein::vdisplay::backends
