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

#ifndef SRC_LOGGING_LOGGING_H_
#define SRC_LOGGING_LOGGING_H_

#include <waypp/waypp.h>

#if BUILD_WAYPP_STANDALONE

#if !defined(NDEBUG)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_OFF
#endif

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog-inl.h>

#define DLOG_DEBUG    SPDLOG_DEBUG
#define DLOG_TRACE    SPDLOG_TRACE
#define DLOG_CRITICAL SPDLOG_CRITICAL

#define LOG_INFO      spdlog::info
#define LOG_DEBUG     spdlog::debug
#define LOG_ERROR     spdlog::error
#define LOG_TRACE     spdlog::trace
#define LOG_WARN      spdlog::warn
#define LOG_CRITICAL  spdlog::critical

class Logging {
public:
    static constexpr int32_t kLogFlushInterval = INT32_C(5);

    Logging() {
        console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        logger_ = std::make_shared<spdlog::logger>("waypp", console_sink_);
        spdlog::set_default_logger(logger_);
        spdlog::set_pattern("[%H:%M:%S.%f] [%L] %v");

        spdlog::flush_on(spdlog::level::err);
        spdlog::flush_every(std::chrono::seconds(kLogFlushInterval));
        spdlog::cfg::load_env_levels();
    }

    ~Logging() = default;

    // Disallow copy and assign.
    Logging(const Logging &) = delete;

    Logging &operator=(const Logging &) = delete;

private:
    std::shared_ptr<spdlog::logger> logger_{};
    std::shared_ptr<
            spdlog::sinks::ansicolor_stdout_sink<spdlog::details::console_mutex>>
            console_sink_;
};

#endif

#endif  // SRC_LOGGING_LOGGING_H_
