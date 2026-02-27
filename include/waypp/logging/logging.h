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

// ---------------------------------------------------------------------------
// Standalone mode  (BUILD_WAYPP_STANDALONE=1, the default for examples)
//
// waypp owns spdlog initialisation.  Set the active log level before pulling
// in any spdlog header so that SPDLOG_DEBUG / SPDLOG_TRACE compile in debug
// builds.  Guard with #ifndef so a parent project that sets the level before
// including this header wins (prevents -Werror=macro-redefinition).
// ---------------------------------------------------------------------------
#if BUILD_WAYPP_STANDALONE

#if !defined(NDEBUG)
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#else
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_OFF
#endif
#endif

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>

/// RAII helper that initialises spdlog once for the process lifetime.
/// Construct one instance in main() before any logging calls.
class Logging {
 public:
  static constexpr int32_t kLogFlushInterval = INT32_C(5);

  Logging() {
    console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    logger_ = std::make_shared<spdlog::logger>("waypp", console_sink_);
    set_default_logger(logger_);
    spdlog::set_pattern("[%H:%M:%S.%f] [%L] %v");
    spdlog::flush_on(spdlog::level::err);
    spdlog::flush_every(std::chrono::seconds(kLogFlushInterval));
    spdlog::cfg::load_env_levels();
  }

  ~Logging() = default;

  Logging(const Logging&) = delete;
  Logging& operator=(const Logging&) = delete;

 private:
  std::shared_ptr<spdlog::logger> logger_{};
  std::shared_ptr<
      spdlog::sinks::ansicolor_stdout_sink<spdlog::details::console_mutex>>
      console_sink_{};
};

// ---------------------------------------------------------------------------
// Embedded mode  (BUILD_WAYPP_STANDALONE=0)
//
// The parent project already owns spdlog — it has set SPDLOG_ACTIVE_LEVEL,
// included spdlog headers, and configured loggers before waypp headers are
// seen.  waypp must NOT redefine SPDLOG_ACTIVE_LEVEL or pull in spdlog sink
// headers (which would fight the parent project's chosen spdlog version /
// configuration).
//
// We still need spdlog/spdlog.h for the SPDLOG_* call-site macros used
// throughout the waypp library source.  That header is safe to include
// multiple times and does not touch logger configuration.
//
// The Logging class is intentionally NOT provided in embedded mode — the
// parent project is responsible for initialising its own logger.  waypp
// source files that hold a `std::unique_ptr<Logging> logging_{}` member will
// still compile because the member is simply `nullptr` (unique_ptr default).
// ---------------------------------------------------------------------------
#else  // !BUILD_WAYPP_STANDALONE

#include <spdlog/spdlog.h>

/// Minimal stub so waypp source files that store
/// `std::unique_ptr<Logging> logging_{}` compile without change.
/// The parent project owns logger initialization; constructing this object
/// is a no-op.
class Logging {
 public:
  Logging() = default;
  ~Logging() = default;
  Logging(const Logging&) = delete;
  Logging& operator=(const Logging&) = delete;
};

#endif  // BUILD_WAYPP_STANDALONE

// ---------------------------------------------------------------------------
// Logging macros — always defined in both modes.
//
// In standalone mode these expand to the full SPDLOG_* call-site macros which
// respect SPDLOG_ACTIVE_LEVEL and embed __FILE__/__LINE__.
// In embedded mode they do the same — the parent project has already set
// SPDLOG_ACTIVE_LEVEL to whatever it needs, so the expansion is correct.
// ---------------------------------------------------------------------------
#define DLOG_DEBUG SPDLOG_DEBUG
#define DLOG_TRACE SPDLOG_TRACE
#define DLOG_CRITICAL SPDLOG_CRITICAL

#define LOG_INFO spdlog::info
#define LOG_DEBUG spdlog::debug
#define LOG_ERROR spdlog::error
#define LOG_TRACE spdlog::trace
#define LOG_WARN spdlog::warn
#define LOG_CRITICAL spdlog::critical

#endif  // SRC_LOGGING_LOGGING_H_
