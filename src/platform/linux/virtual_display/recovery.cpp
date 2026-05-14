/**
 * @file src/platform/linux/virtual_display/recovery.cpp
 * @brief Implementation of boot-time output-restore.
 */
#include "recovery.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <set>
#include <system_error>
#include <vector>

#include <pwd.h>
#include <unistd.h>

#include "src/logging.h"
#include "subprocess.h"

namespace sonnenschein::vdisplay::recovery {

  namespace {

    std::mutex g_mu;

    std::filesystem::path home_dir() {
      if (const char* h = std::getenv("HOME"); h && *h) {
        return h;
      }
      if (auto* pw = getpwuid(geteuid())) {
        return pw->pw_dir;
      }
      return {};
    }

    std::filesystem::path state_dir() {
      // XDG: prefer $XDG_STATE_HOME, else ~/.local/state.
      if (const char* xdg = std::getenv("XDG_STATE_HOME"); xdg && *xdg) {
        return std::filesystem::path(xdg) / "sonnenschein";
      }
      auto h = home_dir();
      if (h.empty()) return {};
      return h / ".local" / "state" / "sonnenschein";
    }

    std::vector<std::string> read_outputs(const std::filesystem::path& path) {
      std::vector<std::string> out;
      std::ifstream f(path);
      if (!f.is_open()) return out;
      std::string line;
      while (std::getline(f, line)) {
        // Trim whitespace.
        auto begin = line.find_first_not_of(" \t\r\n");
        auto end = line.find_last_not_of(" \t\r\n");
        if (begin == std::string::npos) continue;
        out.push_back(line.substr(begin, end - begin + 1));
      }
      return out;
    }

    void write_outputs(
        const std::filesystem::path& path,
        const std::vector<std::string>& outputs) {
      std::error_code ec;
      std::filesystem::create_directories(path.parent_path(), ec);
      if (ec) {
        BOOST_LOG(warning)
            << "Sonnenschein recovery: cannot create state dir "
            << path.parent_path().string() << ": " << ec.message();
        return;
      }
      // Write to a temp file then rename (atomic on POSIX).
      auto tmp = path;
      tmp += ".tmp";
      {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f.is_open()) {
          BOOST_LOG(warning)
              << "Sonnenschein recovery: cannot write " << tmp.string();
          return;
        }
        for (const auto& o : outputs) {
          f << o << "\n";
        }
      }
      std::filesystem::rename(tmp, path, ec);
      if (ec) {
        BOOST_LOG(warning)
            << "Sonnenschein recovery: rename " << tmp.string()
            << " -> " << path.string() << " failed: " << ec.message();
      }
    }

  }  // namespace

  std::filesystem::path lockfile_path() {
    auto dir = state_dir();
    if (dir.empty()) return {};
    return dir / "disabled-outputs.lock";
  }

  void track_disabled_output(const std::string& output_name) {
    if (output_name.empty()) return;
    std::lock_guard<std::mutex> lock(g_mu);
    auto path = lockfile_path();
    if (path.empty()) return;
    auto outputs = read_outputs(path);
    if (std::find(outputs.begin(), outputs.end(), output_name) != outputs.end()) {
      return;  // Already tracked.
    }
    outputs.push_back(output_name);
    write_outputs(path, outputs);
    BOOST_LOG(debug)
        << "Sonnenschein recovery: tracking disabled output '" << output_name
        << "' in " << path.string();
  }

  void untrack_disabled_output(const std::string& output_name) {
    if (output_name.empty()) return;
    std::lock_guard<std::mutex> lock(g_mu);
    auto path = lockfile_path();
    if (path.empty()) return;
    auto outputs = read_outputs(path);
    auto it = std::find(outputs.begin(), outputs.end(), output_name);
    if (it == outputs.end()) return;
    outputs.erase(it);
    std::error_code ec;
    if (outputs.empty()) {
      std::filesystem::remove(path, ec);
      BOOST_LOG(debug)
          << "Sonnenschein recovery: untracked last output, removed "
          << path.string();
    } else {
      write_outputs(path, outputs);
      BOOST_LOG(debug)
          << "Sonnenschein recovery: untracked '" << output_name
          << "' from " << path.string();
    }
  }

  void recover_on_boot() {
    auto path = lockfile_path();
    if (path.empty()) {
      BOOST_LOG(debug) << "Sonnenschein recovery: no state dir resolvable, skipping boot recovery";
      return;
    }
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
      BOOST_LOG(debug)
          << "Sonnenschein recovery: no lockfile at " << path.string()
          << " (last shutdown was clean)";
      return;
    }
    // Read while holding the mutex, but run kscreen-doctor outside the lock.
    std::vector<std::string> outputs;
    {
      std::lock_guard<std::mutex> lock(g_mu);
      outputs = read_outputs(path);
    }
    if (outputs.empty()) {
      // File exists but is empty — delete and bail.
      std::filesystem::remove(path, ec);
      return;
    }
    BOOST_LOG(warning)
        << "Sonnenschein recovery: lockfile found at " << path.string()
        << " — last Sonnenschein session did not exit cleanly. Re-enabling "
        << outputs.size() << " physical output(s).";
    // De-duplicate just in case.
    std::set<std::string> uniq(outputs.begin(), outputs.end());
    for (const auto& out : uniq) {
      BOOST_LOG(info)
          << "Sonnenschein recovery: re-enabling " << out;
      auto res = run_args(
          {"kscreen-doctor", "output." + out + ".enable"},
          "kscreen-doctor enable (recovery)");
      if (!res.ok) {
        BOOST_LOG(warning)
            << "Sonnenschein recovery: enable failed for " << out
            << ": " << res.output;
      }
    }
    // Delete lockfile only after we tried each output. Even if one failed,
    // leaving the lockfile around would cause us to retry forever — better
    // to clear it and let the user re-enable manually if needed.
    std::lock_guard<std::mutex> lock(g_mu);
    std::filesystem::remove(path, ec);
    BOOST_LOG(info) << "Sonnenschein recovery: lockfile cleared";
  }

}  // namespace sonnenschein::vdisplay::recovery
