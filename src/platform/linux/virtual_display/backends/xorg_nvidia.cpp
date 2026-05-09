/**
 * @file src/platform/linux/virtual_display/backends/xorg_nvidia.cpp
 * @brief X11 + NVIDIA proprietary/open driver virtual display (stub).
 *
 * Plan:
 *   - Generate xorg.conf snippet under /etc/X11/xorg.conf.d/20-sonnenschein-virtual.conf
 *     with `--virtual=WxH` plus a Modeline computed via cvt -r WxH HZ.
 *   - Generate a synthetic EDID (libdisplay-info or hand-rolled) advertising
 *     the requested mode + HDR static metadata block when hdr_capable.
 *   - Drop the EDID at /etc/X11/edid/sonnenschein-CLIENT_UID.bin and
 *     reference it via Option "ConnectedMonitor" "DFP-0" + Option
 *     "CustomEDID" "DFP-0:/etc/X11/edid/sonnenschein-...bin".
 *   - On stream start: `xrandr --newmode ...` + `--addmode DFP-0 ...` +
 *     `xrandr --output DFP-0 --mode ...`.
 *   - Reference: docker-steam-headless's
 *     overlay/etc/cont-init.d/70-configure_xorg.sh and
 *     templates/xorg/xorg.dummy.conf, adapted for nvidia-xconfig.
 */
#include "all.h"
#include "../detection.h"

namespace sonnenschein::vdisplay::backends {
  namespace {

    class XorgNvidiaStub : public IBackend {
    public:
      std::string name() const override {
        return "xorg_nvidia";
      }
      std::string display_name() const override {
        return "Xorg + NVIDIA (virtual display via xorg.conf + EDID spoof)";
      }
      bool available(const Environment& env) const override {
        if (env.display_server != DisplayServer::Xorg) {
          return false;
        }
        if (env.primary_gpu_idx < 0) {
          return false;
        }
        return env.gpus[env.primary_gpu_idx].vendor == GpuVendor::Nvidia;
      }
      std::optional<Handle> create(const CreateRequest&) override {
        return std::nullopt;
      }
      void destroy(const std::string&) override {}
      void destroy_all() override {}
    };

  }  // namespace

  std::unique_ptr<IBackend> make_xorg_nvidia() {
    return std::make_unique<XorgNvidiaStub>();
  }

}  // namespace sonnenschein::vdisplay::backends
