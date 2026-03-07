/*
 * Copyright 2024 Joel Winarske
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "command.h"

#include <sys/wait.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstring>

#include "logging/logging.h"

// ─────────────────────────────────────────────────────────────────────────────
// Hard-coded argv tables — one entry per ApprovedCommand value.
//
// Rules:
//   • argv[0] must be the absolute path to the executable.
//   • All arguments are compile-time constants; no runtime string is ever
//     appended, interpolated, or shell-expanded.
//   • The array must be null-terminated (last element == nullptr).
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// argv for ApprovedCommand::kGsettingsGetCursorTheme
// Equivalent to: gsettings get org.gnome.desktop.interface cursor-theme
// but without going through /bin/sh.
constexpr std::array<const char*, 5> kArgvGsettingsGetCursorTheme{{
    "/usr/bin/gsettings",
    "get",
    "org.gnome.desktop.interface",
    "cursor-theme",
    nullptr,
}};

/**
 * @brief Return the hard-coded argv for the given ApprovedCommand.
 *
 * Returns nullptr if the enum value is not recognised (should never happen
 * in practice, but keeps the switch exhaustive and avoids UB).
 */
const char* const* argv_for(ApprovedCommand cmd) noexcept {
  switch (cmd) {
    case ApprovedCommand::kGsettingsGetCursorTheme:
      return kArgvGsettingsGetCursorTheme.data();
  }
  return nullptr;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// RunApproved — pipe + fork + execve, no shell intermediary
// ─────────────────────────────────────────────────────────────────────────────
bool Command::RunApproved(ApprovedCommand cmd, std::string& result) {
  result.clear();

  const char* const* argv = argv_for(cmd);
  if (!argv || !argv[0]) {
    LOG_ERROR("[Command] RunApproved: unknown ApprovedCommand value");
    return false;
  }

  // Create a pipe: pipefd[0] = read end, pipefd[1] = write end.
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    LOG_ERROR("[Command] RunApproved: pipe() failed: {}", std::strerror(errno));
    return false;
  }

  const pid_t pid = fork();

  if (pid < 0) {
    // fork() failed — clean up both pipe ends and bail.
    LOG_ERROR("[Command] RunApproved: fork() failed: {}", std::strerror(errno));
    close(pipefd[0]);
    close(pipefd[1]);
    return false;
  }

  if (pid == 0) {
    // ── Child process ───────────────────────────────────────────────────────
    // Redirect stdout → write the end of pipe, then exec.
    // Any failure here calls _exit() so C++ destructors are NOT run in the
    // child (avoids double-free of shared resources).
    close(pipefd[0]);  // a child does not read

    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
      _exit(127);
    }
    close(pipefd[1]);

    // execve: no shell, no PATH search, no caller-controlled strings.
    // The const_cast is required by the POSIX execve signature; the arrays
    // themselves are immutable compile-time constants.
    execve(argv[0], const_cast<char* const*>(argv),
           nullptr /* empty environment */);

    // execve only returns on failure.
    _exit(127);
  }

  // ── Parent process ─────────────────────────────────────────────────────────
  close(pipefd[1]);  // parent does not write

  // Drain the child's stdout into a result.
  {
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
      result.append(buf, static_cast<std::size_t>(n));
    }
    if (n < 0) {
      LOG_ERROR("[Command] RunApproved: read() failed: {}",
                std::strerror(errno));
    }
  }
  close(pipefd[0]);

  // Reap the child and inspect its exit status.
  int wstatus = 0;
  if (waitpid(pid, &wstatus, 0) == -1) {
    LOG_ERROR("[Command] RunApproved: waitpid() failed: {}",
              std::strerror(errno));
    return false;
  }

  if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
    LOG_ERROR("[Command] RunApproved: '{}' exited with status {}", argv[0],
              WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1);
    return false;
  }

  DLOG_TRACE("[Command] RunApproved: '{}' succeeded, {} byte(s)", argv[0],
             result.size());
  return true;
}