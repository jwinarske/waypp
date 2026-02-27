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

#include <waypp/waypp.h>

#if ENABLE_IVI_SHELL_CLIENT

#include <cstdint>
#include <functional>
#include <memory>

#if ENABLE_EGL
#include "waypp/window/egl.h"
#endif
#include "window_manager.h"

class IviSurface;

/**
 * @class IviWindowManager
 *
 * @brief Manages application windows using the IVI-shell protocol.
 *
 * IviWindowManager is responsible for creating and managing IVI surfaces
 * on compositors that support the ivi_application Wayland protocol.
 * It inherits from WindowManager (which inherits from Registrar) and
 * follows the same pattern as XdgWindowManager.
 *
 * The compositor must advertise the ivi_application global; if it does not,
 * the constructor throws std::runtime_error.
 */
class IviWindowManager : public std::enable_shared_from_this<IviWindowManager>,
                         public WindowManager {
 public:
  /**
   * @brief Construct an IviWindowManager.
   *
   * @param display           The wl_display connection to the compositor.
   * @param disable_cursor    If true, the default pointer cursor is hidden.
   * @param ext_interface_count  Number of additional Wayland interface
   *                             callbacks to register (pass 0 for none).
   * @param ext_interface_data   Pointer to an array of RegistrarCallback
   *                             structs (pass nullptr for none).
   * @param context           Optional GMainContext for event dispatching.
   *
   * @throws std::runtime_error if the compositor does not support
   *         ivi_application.
   */
  explicit IviWindowManager(
      wl_display* display,
      bool disable_cursor = false,
      unsigned long ext_interface_count = 0,
      const RegistrarCallback* ext_interface_data = nullptr,
      GMainContext* context = nullptr);

  ~IviWindowManager();

  /**
   * @brief Create an IVI surface with the given IVI surface ID.
   *
   * The IVI surface ID must be unique across all IVI surfaces on the
   * compositor. Callers are responsible for allocating IDs from a
   * system-wide registry or manifest to avoid conflicts.
   *
   * @param ivi_id         IVI surface identifier assigned by the application.
   * @param width          Initial surface width in pixels.
   * @param height         Initial surface height in pixels.
   * @param buffer_count   Number of backing buffers (typically 2 for
   *                       double-buffering).
   * @param buffer_format  wl_shm pixel format (e.g. WL_SHM_FORMAT_XRGB8888).
   * @param frame_callback Callback invoked on every frame, receiving a user
   *                       data pointer and the frame timestamp in ms.
   * @param egl_config     Optional EGL configuration; pass nullptr for SHM
   *                       (software) rendering.
   * @return A shared_ptr to the newly created IviSurface.
   */
  std::shared_ptr<IviSurface> create_ivi_surface(
      uint32_t ivi_id,
      int width,
      int height,
      int buffer_count,
      uint32_t buffer_format,
      const std::function<void(void*, uint32_t)>& frame_callback,
#if ENABLE_EGL
      waypp::Egl::config* egl_config = nullptr
#else
      void* egl_config = nullptr
#endif
  );

  // Disallow copy and assign.
  IviWindowManager(const IviWindowManager&) = delete;

  IviWindowManager& operator=(const IviWindowManager&) = delete;

 private:
  /// Cached pointer to the ivi_application global (owned by Registrar).
  ivi_application* ivi_application_{};

  /// The most recently created IVI surface (retained to keep it alive).
  std::shared_ptr<IviSurface> ivi_surface_;
};

#endif  // ENABLE_IVI_SHELL_CLIENT
