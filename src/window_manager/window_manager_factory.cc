/*
 * Copyright 2026 Joel Winarske
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

#include "waypp/window_manager/window_manager_factory.h"

#include <stdexcept>

#include "logging/logging.h"

/**
 * @brief Create the highest-priority window manager available.
 *
 * Priority (compile-time):
 *   1. AglShell         — ENABLE_AGL_SHELL_CLIENT && ENABLE_XDG_CLIENT
 *   2. IviWindowManager — ENABLE_IVI_SHELL_CLIENT
 *   3. XdgWindowManager — ENABLE_XDG_CLIENT
 *
 * If none of the above are enabled at compile time this function throws
 * std::runtime_error so that misconfigured builds fail at a clear call site
 * rather than silently doing nothing.
 */
WindowManagerFactory::Result WindowManagerFactory::create(
    wl_display* display,
    const bool disable_cursor,
    const unsigned long ext_interface_count,
    const Registrar::RegistrarCallback* ext_interface_data,
    GMainContext* context) {
  DLOG_TRACE("WindowManagerFactory::create()");

  // ── Priority 1: AGL shell (superset of XDG) ────────────────────────────
#if ENABLE_AGL_SHELL_CLIENT && ENABLE_XDG_CLIENT
  DLOG_DEBUG("WindowManagerFactory: attempting AglShell");
  try {
    const auto wm =
        std::make_shared<AglShell>(display, disable_cursor, ext_interface_count,
                                   ext_interface_data, context);
    LOG_INFO("WindowManagerFactory: created AglShell");
    return {std::static_pointer_cast<WindowManager>(wm),
            WindowManagerType::kAgl};
  } catch (const std::runtime_error& e) {
  }
#endif  // ENABLE_AGL_SHELL_CLIENT && ENABLE_XDG_CLIENT

  // ── Priority 2: IVI shell ───────────────────────────────────────────────
#if ENABLE_IVI_SHELL_CLIENT
  DLOG_DEBUG("WindowManagerFactory: attempting IviWindowManager");
  try {
    const auto wm = std::make_shared<IviWindowManager>(
        display, disable_cursor, ext_interface_count, ext_interface_data,
        context);
    LOG_INFO("WindowManagerFactory: created IviWindowManager");
    return {std::static_pointer_cast<WindowManager>(wm),
            WindowManagerType::kIvi};
  } catch (const std::runtime_error& e) {
  }
#endif  // ENABLE_IVI_SHELL_CLIENT

  // ── Priority 3: XDG shell ───────────────────────────────────────────────
#if ENABLE_XDG_CLIENT
  DLOG_DEBUG("WindowManagerFactory: attempting XdgWindowManager");
  const auto wm = std::make_shared<XdgWindowManager>(
      display, disable_cursor, ext_interface_count, ext_interface_data,
      context);
  LOG_INFO("WindowManagerFactory: created XdgWindowManager");
  return {std::static_pointer_cast<WindowManager>(wm), WindowManagerType::kXdg};
#endif  // ENABLE_XDG_CLIENT

  // ── No shell available ──────────────────────────────────────────────────
  throw std::runtime_error(
      "WindowManagerFactory: no supported shell protocol is enabled "
      "(ENABLE_XDG_CLIENT, ENABLE_AGL_SHELL_CLIENT, ENABLE_IVI_SHELL_CLIENT)");
}
