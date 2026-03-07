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

#pragma once

#include <string>

/**
 * @brief Enumeration of every subprocess that waypp is permitted to run.
 *
 * Adding a new external process requires explicitly extending this enum
 * *and* the corresponding argv table in command.cc.  No caller can pass an
 * arbitrary string to the execution layer.
 */
enum class ApprovedCommand {
  /// gsettings get org.gnome.desktop.interface cursor-theme
  kGsettingsGetCursorTheme,
};

/**
 * @class Command
 *
 * @brief Shell-free subprocess runner.
 *
 * Every execution path uses @c pipe(2) + @c fork(2) + @c execve(2) with a
 * hard-coded, null-terminated argv array.  There is no shell intermediary,
 * no string sanitisation step, and no caller-controlled argument.
 *
 * Usage:
 * @code
 *   std::string theme;
 *   Command::RunApproved(ApprovedCommand::kGsettingsGetCursorTheme, theme);
 * @endcode
 */
class Command {
 public:
  /**
   * @brief Execute a pre-approved command and capture its stdout.
   *
   * Forks a child, exec's the hard-coded argv for @p cmd (no shell), reads
   * all output from the child's stdout into @p result, then reaps the child.
   *
   * @param cmd   Which pre-approved command to run.
   * @param result  Receives the child's stdout, cleared on entry.
   * @return @c true if the child exited with status 0 and stdout was captured;
   *         @c false on fork/exec/wait failure or non-zero exit status.
   */
  static bool RunApproved(ApprovedCommand cmd, std::string& result);
};
