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

#include <cstring>

#include "logging/logging.h"

bool Command::is_safe_char(const char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '_' ||
         c == '-' || c == '/' || c == '.';
}

std::string Command::sanitize_cmd(const std::string& cmd) {
  std::string safe_cmd;
  for (const char c : cmd) {
    if (is_safe_char(c)) {
      safe_cmd += c;
    }
  }
  return safe_cmd;
}

bool Command::Execute(const std::string& cmd, std::string& result) {
  if (cmd.empty()) {
    spdlog::error("[Command] Execute: cmd is empty");
    return false;
  }
  const std::string safe_cmd = sanitize_cmd(cmd);
  if (safe_cmd.empty()) {
    spdlog::error(
        "[Command] Execute: command '{}' reduced to empty string "
        "after sanitization — refusing to execute",
        cmd);
    return false;
  }
  FILE* fp = popen(safe_cmd.c_str(), "r");
  if (!fp) {
    spdlog::error("[ExecuteCommand] Failed to Execute Command: ({}) {}", errno,
                  strerror(errno));
    spdlog::error("Failed to Execute Command: {}", cmd);
    return false;
  }

  SPDLOG_TRACE("[Command] Execute: {}", cmd);

  result.clear();
  auto buf = std::make_unique<char[]>(1024);
  while (fgets(buf.get(), 1024, fp) != nullptr) {
    result.append(buf.get());
  }
  buf.reset();

  SPDLOG_TRACE("[Command] Execute Result: [{}] {}", result.size(), result);

  if (pclose(fp) == -1) {
    spdlog::error("[ExecuteCommand] Failed to Close Pipe: ({}) {}", errno,
                  strerror(errno));
    return false;
  }
  return true;
}