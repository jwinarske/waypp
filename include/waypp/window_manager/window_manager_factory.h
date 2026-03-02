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

#pragma once

#include <memory>

#include "registrar.h"
#include "waypp/waypp.h"
#include "window_manager.h"

#if ENABLE_AGL_SHELL_CLIENT
#include "agl_shell.h"
#endif

#if ENABLE_IVI_SHELL_CLIENT
#include "ivi_window_manager.h"
#endif

#if ENABLE_XDG_CLIENT
#include "xdg_window_manager.h"
#endif

/**
 * @enum WindowManagerType
 *
 * @brief Identifies which shell-protocol backend is in use.
 *
 * The active type is determined at construction time by
 * WindowManagerFactory::create() based on compile-time feature flags and the
 * protocols advertised by the compositor at runtime.
 *
 * Priority (highest → lowest):
 *   1. AGL shell  — when ENABLE_AGL_SHELL_CLIENT && ENABLE_XDG_CLIENT
 *   2. IVI shell  — when ENABLE_IVI_SHELL_CLIENT (and AGL is absent)
 *   3. XDG shell  — otherwise
 */
enum class WindowManagerType {
  kXdg,  ///< XDG-shell window manager (xdg_wm_base)
  kAgl,  ///< AGL shell window manager (agl_shell + xdg_wm_base)
  kIvi,  ///< IVI-shell window manager (ivi_application)
};

/**
 * @class WindowManagerFactory
 *
 * @brief Creates the appropriate WindowManager concrete type at runtime.
 *
 * Selection rules (evaluated in order):
 *
 *  - If both ENABLE_AGL_SHELL_CLIENT and ENABLE_XDG_CLIENT are set, an
 *    AglShell instance is created (AGL is an XDG superset).
 *
 *  - Else if ENABLE_IVI_SHELL_CLIENT is set, an IviWindowManager is created.
 *
 *  - Otherwise an XdgWindowManager is created.
 *
 * The factory returns a std::shared_ptr<WindowManager> for the base type
 * alongside a WindowManagerType tag so callers can safely downcast when
 * shell-specific APIs are needed.
 *
 * @note All selection logic is resolved at compile time via preprocessor
 *       guards; no runtime vtable dispatch overhead is introduced.
 *
 * Usage example:
 * @code
 *   auto [wm, type] = WindowManagerFactory::create(display, disable_cursor);
 *
 *   if (type == WindowManagerType::kAgl) {
 *     auto agl = std::static_pointer_cast<AglShell>(wm);
 *     agl->ready();
 *   }
 * @endcode
 */
class WindowManagerFactory {
 public:
  /**
   * @brief Result bundle returned by create().
   */
  struct Result {
    /// The concrete window-manager instance (AglShell, IviWindowManager, or
    /// XdgWindowManager) held via its WindowManager base pointer.
    std::shared_ptr<WindowManager> window_manager;

    /// Tag identifying which concrete type was instantiated.
    WindowManagerType type;
  };

  /**
   * @brief Instantiate the highest-priority available window manager.
   *
   * @param display              An open wl_display connection.
   * @param disable_cursor       If true, the default pointer cursor is hidden.
   * @param ext_interface_count  Number of additional RegistrarCallback entries
   *                             (pass 0 for none).
   * @param ext_interface_data   Pointer to an array of RegistrarCallback
   *                             structs (pass nullptr for none).
   * @param context              Optional GMainContext for event dispatching.
   *
   * @return A Result containing the window manager and its type tag.
   *
   * @throws std::runtime_error if no supported shell protocol is available.
   */
  [[nodiscard]] static Result create(
      wl_display* display,
      bool disable_cursor = false,
      unsigned long ext_interface_count = 0,
      const Registrar::RegistrarCallback* ext_interface_data = nullptr,
      GMainContext* context = nullptr);

  // Factory — not instantiable.
  WindowManagerFactory() = delete;
};
