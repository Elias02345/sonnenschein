/**
 * @file src/platform/linux/virtual_display/detection.h
 * @brief Runtime detection of display server, compositor, and GPUs.
 *
 * Cheap to call (does some sysfs walks and one or two short subprocess
 * invocations like `nvidia-smi`). Cache the result; don't call this in
 * hot paths.
 */
#pragma once

#include "types.h"

#include <string>

namespace sonnenschein::vdisplay {

  /// Inspect environment variables, /sys/class/drm, /proc/driver/nvidia,
  /// and (on Wayland) the running compositor process to fill an Environment.
  ///
  /// Never throws — on full detection failure returns an Environment with
  /// DisplayServer::None, Compositor::Unknown, empty gpus, primary_gpu_idx -1.
  [[nodiscard]] Environment detect();

  // Stringify helpers for logs and the WebUI diagnostics tab.
  [[nodiscard]] std::string to_string(DisplayServer);
  [[nodiscard]] std::string to_string(Compositor);
  [[nodiscard]] std::string to_string(GpuVendor);

}  // namespace sonnenschein::vdisplay
