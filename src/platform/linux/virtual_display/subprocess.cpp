/**
 * @file src/platform/linux/virtual_display/subprocess.cpp
 */
#include "subprocess.h"

#include "src/logging.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace sonnenschein::vdisplay {

  namespace {

    constexpr size_t kMaxCaptureBytes = 64 * 1024;

    /// Drain the FILE* until EOF or kMaxCaptureBytes, whichever comes first.
    std::string drain(FILE* fp) {
      std::array<char, 4096> buf{};
      std::string out;
      while (auto n = std::fread(buf.data(), 1, buf.size(), fp)) {
        out.append(buf.data(), n);
        if (out.size() >= kMaxCaptureBytes) {
          out.resize(kMaxCaptureBytes);
          out.append("\n[output truncated at 64 KB]\n");
          // Drain the rest so the child can exit cleanly.
          while (std::fread(buf.data(), 1, buf.size(), fp)) {
          }
          break;
        }
      }
      return out;
    }

  }  // namespace

  SubprocessResult run_shell(const std::string& command,
                             const std::string& description) {
    SubprocessResult res;
    // Combine stderr into stdout so we capture both via popen's read pipe.
    const std::string full = command + " 2>&1";

    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(full.c_str(), "r"), pclose);
    if (!pipe) {
      BOOST_LOG(warning)
          << "Sonnenschein vdisplay: failed to spawn shell for '"
          << description << "': " << std::strerror(errno);
      return res;
    }

    res.output = drain(pipe.get());
    // pclose returns wstatus
    int wstatus = pclose(pipe.release());
    if (wstatus == -1) {
      BOOST_LOG(warning)
          << "Sonnenschein vdisplay: pclose failed for '" << description
          << "': " << std::strerror(errno);
      return res;
    }
    if (WIFEXITED(wstatus)) {
      res.exit_code = WEXITSTATUS(wstatus);
      res.ok = (res.exit_code == 0);
    } else if (WIFSIGNALED(wstatus)) {
      res.exit_code = 128 + WTERMSIG(wstatus);
    }

    if (!res.ok) {
      BOOST_LOG(debug)
          << "Sonnenschein vdisplay: '" << description
          << "' exit=" << res.exit_code
          << " output=" << res.output;
    }
    return res;
  }

  SubprocessResult run_args(const std::vector<std::string>& argv,
                            const std::string& description) {
    SubprocessResult res;
    if (argv.empty()) {
      BOOST_LOG(warning)
          << "Sonnenschein vdisplay: run_args called with empty argv for '"
          << description << "'";
      return res;
    }

    int pipe_fds[2] = {-1, -1};
    if (pipe(pipe_fds) != 0) {
      BOOST_LOG(warning)
          << "Sonnenschein vdisplay: pipe() failed for '" << description
          << "': " << std::strerror(errno);
      return res;
    }

    pid_t pid = fork();
    if (pid == -1) {
      BOOST_LOG(warning)
          << "Sonnenschein vdisplay: fork() failed for '" << description
          << "': " << std::strerror(errno);
      ::close(pipe_fds[0]);
      ::close(pipe_fds[1]);
      return res;
    }

    if (pid == 0) {
      // child
      ::close(pipe_fds[0]);
      // route stdout + stderr to the pipe
      dup2(pipe_fds[1], STDOUT_FILENO);
      dup2(pipe_fds[1], STDERR_FILENO);
      ::close(pipe_fds[1]);

      // build argv with c-strings
      std::vector<char*> c_argv;
      c_argv.reserve(argv.size() + 1);
      for (const auto& s : argv) {
        c_argv.push_back(const_cast<char*>(s.c_str()));
      }
      c_argv.push_back(nullptr);

      execvp(c_argv[0], c_argv.data());
      // exec failed
      std::string err = "execvp failed: ";
      err += std::strerror(errno);
      err += "\n";
      // Write best-effort to the pipe so the parent sees it.
      auto _ignored = ::write(STDERR_FILENO, err.c_str(), err.size());
      (void)_ignored;
      _exit(127);
    }

    // parent
    ::close(pipe_fds[1]);
    std::unique_ptr<FILE, int(*)(FILE*)> rfp(fdopen(pipe_fds[0], "r"), [](FILE* f) {
      return f ? std::fclose(f) : 0;
    });
    if (!rfp) {
      BOOST_LOG(warning)
          << "Sonnenschein vdisplay: fdopen failed for '" << description << "'";
      ::close(pipe_fds[0]);
      // Reap child.
      waitpid(pid, nullptr, 0);
      return res;
    }
    res.output = drain(rfp.get());

    int wstatus = 0;
    if (waitpid(pid, &wstatus, 0) == -1) {
      BOOST_LOG(warning)
          << "Sonnenschein vdisplay: waitpid failed for '" << description
          << "': " << std::strerror(errno);
      return res;
    }
    if (WIFEXITED(wstatus)) {
      res.exit_code = WEXITSTATUS(wstatus);
      res.ok = (res.exit_code == 0);
    } else if (WIFSIGNALED(wstatus)) {
      res.exit_code = 128 + WTERMSIG(wstatus);
    }

    if (!res.ok) {
      BOOST_LOG(debug)
          << "Sonnenschein vdisplay: '" << description
          << "' exit=" << res.exit_code
          << " output=" << res.output;
    }
    return res;
  }

}  // namespace sonnenschein::vdisplay
