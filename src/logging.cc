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

#include "logging.h"

#include "config.h"

static constexpr int32_t kLogFlushInterval = INT32_C(5);

Logging::Logging() {
    console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    logger_ = std::make_shared<spdlog::logger>("waypp", console_sink_);
    spdlog::set_default_logger(logger_);
    spdlog::set_pattern("[%H:%M:%S.%f] [%L] %v");

    spdlog::flush_on(spdlog::level::err);
    spdlog::flush_every(std::chrono::seconds(kLogFlushInterval));
    spdlog::cfg::load_env_levels();
}

Logging::~Logging() {}
