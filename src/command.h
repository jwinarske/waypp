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

class Command {
 public:
  /**
   * @brief Executes a command and captures its output.
   *
   * Sanitizes the input command, executes it using `popen`, and captures
   * the output. Logs errors if the command execution or pipe closure fails.
   *
   * @param cmd The command to execute.
   * @param result A string to store the command's output.
   * @return True if the command executed successfully, false otherwise.
   */
  static bool Execute(const std::string& cmd, std::string& result);

 private:
  /**
   * @brief Checks if a character is safe to use in a command.
   *
   * A character is considered safe if it is alphanumeric, a space,
   * an underscore, a hyphen, a forward slash, or a period.
   *
   * @param c The character to check.
   * @return Non-zero value if the character is safe, zero otherwise.
   */
  static int is_safe_char(char c);

  /**
   * @brief Sanitizes a command string by removing unsafe characters.
   *
   * Iterates through the input command string and appends only safe
   * characters to the output string.
   *
   * @param cmd The command string to sanitize.
   * @return A sanitized command string containing only safe characters.
   */
  static std::string sanitize_cmd(const std::string& cmd);
};
