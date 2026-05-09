/**
 * @file src/platform/linux/virtual_display/backends/xorg_dummy.cpp
 * @brief X11 + xf86-video-dummy backend for AMD/Intel (stub).
 *
 * Plan: docker-steam-headless's xorg.dummy.conf is the reference. We
 * generate a per-resolution Modeline via `cvt -r WxH HZ`, drop a config
 * fragment under /etc/X11/xorg.conf.d/, and use `xrandr --newmode` +
 * `--addmode` + `--output DUMMY0 --mode` to switch on demand.
 *
 * VRAM-cap caveat: dummy driver advertises a fixed VideoRam value (256MB
 * by default in the reference config). For 4K@120 HDR streams we bump it
 * to 1024MB. Any GLX rendering is software-only on dummy — for hardware
 * accel on AMD/Intel we rely on the amdgpu_param backend instead. This
 * one is a fallback for very old hardware or restricted setups.
 */
#include "all.h"
#include "../detection.h"

namespace sonnenschein::vdisplay::backends {
  namespace {

    class XorgDummyStub : public IBackend {
    public:
      std::string name() const override {
        return "xorg_dummy";
      }
      std::string display_name() const override {
        return "Xorg + dummy driver (AMD/Intel software fallback)";
      }
      bool available(const Environment& env) const override {
        if (env.display_server != DisplayServer::Xorg) {
          return false;
        }
        if (env.primary_gpu_idx < 0) {
          return false;
        }
        const auto v = env.gpus[env.primary_gpu_idx].vendor;
        return v == GpuVendor::Amd || v == GpuVendor::Intel;
      }
      std::optional<Handle> create(const CreateRequest&) override {
        return std::nullopt;
      }
      void destroy(const std::string&) override {}
      void destroy_all() override {}
    };

  }  // namespace

  std::unique_ptr<IBackend> make_xorg_dummy() {
    return std::make_unique<XorgDummyStub>();
  }

}  // namespace sonnenschein::vdisplay::backends
