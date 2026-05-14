/**
 * @file src/platform/linux/virtual_display/recovery.h
 * @brief Boot-time output-restore mechanism.
 *
 * Sonnenschein disables the user's physical monitors when a streaming
 * session starts (priority-1 headless mode) and re-enables them on
 * normal shutdown. If the process dies abnormally (SIGKILL, OOM-killer,
 * power loss) before the destroy_all() path runs, the monitors stay
 * off — leaving the user blind until manual recovery.
 *
 * This module solves that by persisting the list of disabled outputs to
 * a lockfile while a session is active. On the next Sonnenschein start,
 * recover_on_boot() reads the lockfile, runs `kscreen-doctor output.X.enable`
 * for every entry, and deletes the file. Sonnenschein heals itself on
 * the next launch even after a hard kill.
 */
#pragma once

#include <filesystem>
#include <string>

namespace sonnenschein::vdisplay::recovery {

  /// ~/.local/state/sonnenschein/disabled-outputs.lock
  std::filesystem::path lockfile_path();

  /// Append `output_name` to the lockfile, creating the parent dir if needed.
  /// Idempotent: if the output is already listed, no-op. Safe to call from
  /// multiple threads (uses an in-process mutex; the lockfile is process-local
  /// and not meant to coordinate between processes).
  void track_disabled_output(const std::string& output_name);

  /// Remove `output_name` from the lockfile. If the file becomes empty,
  /// delete it. Idempotent.
  void untrack_disabled_output(const std::string& output_name);

  /// On Sonnenschein start: read lockfile, run `kscreen-doctor output.X.enable`
  /// for every listed output, then delete the lockfile. Logs each step.
  /// No-op if the lockfile does not exist.
  ///
  /// Must be called AFTER logging is initialized but BEFORE the main loop —
  /// see src/main.cpp.
  void recover_on_boot();

}  // namespace sonnenschein::vdisplay::recovery
