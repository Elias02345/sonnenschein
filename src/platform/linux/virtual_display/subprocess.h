/**
 * @file src/platform/linux/virtual_display/subprocess.h
 * @brief Tiny popen-based subprocess helper for the virtual-display backends.
 *
 * Backends frequently shell out to small CLI tools (kscreen-doctor,
 * xrandr, swaymsg, gdbus). We need:
 *   - capture stdout AND stderr (kscreen-doctor reports errors on stderr)
 *   - bounded read size so a runaway tool can't OOM us
 *   - exit code visibility for "did this command actually succeed"
 *   - clean BOOST_LOG diagnostics on failure
 *
 * For per-call timeouts we'd want fork+exec+poll; that's overkill for
 * the simple "tell KWin to add a virtual output" case. Add it later if
 * a backend needs it (start with kwin_wayland — kscreen-doctor returns
 * promptly).
 */
#pragma once

#include <string>
#include <vector>

namespace sonnenschein::vdisplay {

  struct SubprocessResult {
    /// True iff the process exited normally with status 0.
    bool ok = false;

    /// Combined stdout + stderr (truncated to ~64 KB).
    std::string output;

    /// Raw exit code (-1 if the process couldn't be spawned).
    int exit_code = -1;
  };

  /// Run `command` (already quoted) via /bin/sh -c, capture combined
  /// stdout+stderr, return result.
  ///
  /// `description` is what we'll log on failure ("kscreen-doctor add-virtual-output").
  /// Caller is responsible for argument quoting — for safety, prefer the
  /// run_args() variant when interpolating user data.
  [[nodiscard]] SubprocessResult run_shell(const std::string& command,
                                           const std::string& description);

  /// Run an argv-style command (no shell, no quoting headaches). The first
  /// element is the executable, the rest are its arguments. Each is passed
  /// to execvp() unchanged — safe to interpolate untrusted strings.
  ///
  /// Returns SubprocessResult.exit_code = -1 if fork or exec fails.
  [[nodiscard]] SubprocessResult run_args(const std::vector<std::string>& argv,
                                          const std::string& description);

}  // namespace sonnenschein::vdisplay
