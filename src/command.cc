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

#include <memory>
#include <cstring>

#include "logging/logging.h"

bool Command::Execute(const char *cmd, std::string &result) {
    auto fp = popen(cmd, "r");
    if (!fp) {
        LOG_ERROR("[ExecuteCommand] Failed to Execute Command: ({}) {}", errno,
                      std::strerror(errno));
        LOG_ERROR("Failed to Execute Command: {}", cmd);
        return false;
    }

    DLOG_TRACE("[Command] Execute: {}", cmd);

    auto buf = std::make_unique<char[]>(1024);
    while (fgets(&buf[0], 1024, fp) != nullptr) {
        result.append(&buf[0]);
    }
    buf.reset();

    DLOG_TRACE("[Command] Execute Result: [{}] {}", result.size(), result);

    auto status = pclose(fp);
    if (status == -1) {
        LOG_ERROR("[ExecuteCommand] Failed to Close Pipe: ({}) {}", errno,
                      std::strerror(errno));
        return false;
    }
    return true;
}
