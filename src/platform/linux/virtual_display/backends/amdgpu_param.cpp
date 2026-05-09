/**
 * @file src/platform/linux/virtual_display/backends/amdgpu_param.cpp
 * @brief AMD-only backend using the `amdgpu.virtual_display` kernel param (stub).
 *
 * Plan: AMD's amdgpu kernel module accepts
 *     amdgpu.virtual_display=PCI:x:y.z,N
 * which exposes N additional virtual CRTCs on the given PCI device. These
 * CRTCs render through the GPU like any real one — full hardware accel,
 * VAAPI encoding, no dummy fallback. Best AMD path.
 *
 * The installer (Phase 3) writes /etc/modprobe.d/sonnenschein-amdgpu.conf
 * once (so the param survives reboots), and triggers a kernel param
 * reload via `update-initramfs` / `mkinitcpio` / `dracut` per distro.
 *
 * At runtime this backend just calls xrandr / wlr-output-management to
 * switch the virtual CRTC on with the requested mode.
 */
#include "all.h"
#include "../detection.h"

namespace sonnenschein::vdisplay::backends {
  namespace {

    class AmdgpuParamStub : public IBackend {
    public:
      std::string name() const override {
        return "amdgpu_param";
      }
      std::string display_name() const override {
        return "AMD (amdgpu.virtual_display kernel parameter)";
      }
      bool available(const Environment& env) const override {
        // Available as long as at least one AMD GPU is present. The actual
        // kernel-param installer step is checked separately at create() time.
        for (const auto& g : env.gpus) {
          if (g.vendor == GpuVendor::Amd) {
            return true;
          }
        }
        return false;
      }
      std::optional<Handle> create(const CreateRequest&) override {
        return std::nullopt;
      }
      void destroy(const std::string&) override {}
      void destroy_all() override {}
    };

  }  // namespace

  std::unique_ptr<IBackend> make_amdgpu_param() {
    return std::make_unique<AmdgpuParamStub>();
  }

}  // namespace sonnenschein::vdisplay::backends
