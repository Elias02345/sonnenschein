/**
 * @file src/platform/linux/virtual_display/backends/evdi.cpp
 * @brief EVDI (Extensible Virtual Display Interface) DKMS backend (stub).
 *
 * Plan: load `evdi` kernel module, then for each create() open a new
 * /dev/dri/cardN-EVDI device and feed it framebuffer pages via the
 * libevdi userspace library. Vendor-agnostic, but the DKMS module is an
 * extra dependency we'd rather not push on every user.
 *
 * Used when nothing else is available: e.g. an old Intel iGPU on Xorg
 * where dummy driver is too limited, or a NVIDIA + Wayland combo where
 * KWin virtual outputs aren't usable.
 */
#include "all.h"
#include "../detection.h"

namespace sonnenschein::vdisplay::backends {
  namespace {

    class EvdiStub : public IBackend {
    public:
      std::string name() const override {
        return "evdi";
      }
      std::string display_name() const override {
        return "EVDI (universal fallback, requires DKMS module)";
      }
      bool available(const Environment&) const override {
        // Phase 2A: always reports false. The real implementation will check
        // for /dev/dri/card-EVDI* or the loaded `evdi` module. Backend acts
        // as last-resort, so factory only picks it when explicitly requested.
        return false;
      }
      std::optional<Handle> create(const CreateRequest&) override {
        return std::nullopt;
      }
      void destroy(const std::string&) override {}
      void destroy_all() override {}
    };

  }  // namespace

  std::unique_ptr<IBackend> make_evdi() {
    return std::make_unique<EvdiStub>();
  }

}  // namespace sonnenschein::vdisplay::backends
