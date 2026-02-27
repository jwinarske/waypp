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

#include "waypp/window_manager/ivi_window_manager.h"

#if ENABLE_IVI_SHELL_CLIENT

#include <stdexcept>

#include "logging/logging.h"
#include "waypp/window/ivi_surface.h"
using waypp::Egl;

/**
 * @class IviWindowManager
 *
 * @brief Manages application windows using the IVI-shell protocol.
 *
 * The IviWindowManager class initialises the Wayland registry via its
 * WindowManager base class, then retrieves the ivi_application global
 * advertised by the compositor. Applications call create_ivi_surface() to
 * create an IVI surface with a caller-assigned numeric surface ID.
 */
IviWindowManager::IviWindowManager(wl_display* display,
                                   const bool disable_cursor,
                                   const unsigned long ext_interface_count,
                                   const RegistrarCallback* ext_interface_data,
                                   GMainContext* context)
    : WindowManager(display,
                    disable_cursor,
                    ext_interface_count,
                    ext_interface_data,
                    context) {
  DLOG_TRACE("++IviWindowManager::IviWindowManager()");

  ivi_application_ = get_ivi_application();
  if (!ivi_application_) {
    throw std::runtime_error(
        "IVI Application (ivi_application) is not supported by the compositor");
  }

  DLOG_TRACE("--IviWindowManager::IviWindowManager()");
}

IviWindowManager::~IviWindowManager() = default;

/**
 * @brief Create an IVI surface with the given IVI surface ID.
 *
 * Constructs an IviSurface object, which calls ivi_application_surface_create()
 * internally to register the surface with the compositor. The returned
 * shared_ptr keeps the surface alive for the lifetime of the caller's handle.
 *
 * @param ivi_id         IVI surface identifier (must be unique on this
 * display).
 * @param width          Initial surface width in pixels.
 * @param height         Initial surface height in pixels.
 * @param buffer_count   Number of backing buffers.
 * @param buffer_format  wl_shm pixel format.
 * @param frame_callback Invoked each frame with (user_data, timestamp_ms).
 * @param egl_config     EGL configuration, or nullptr for SHM rendering.
 * @return Shared pointer to the created IviSurface.
 */
std::shared_ptr<IviSurface> IviWindowManager::create_ivi_surface(
    const uint32_t ivi_id,
    const int width,
    const int height,
    const int buffer_count,
    const uint32_t buffer_format,
    const std::function<void(void*, uint32_t)>& frame_callback,
#if ENABLE_EGL
    Egl::config* egl_config
#else
    void* egl_config
#endif
) {
  ivi_surface_ = std::make_shared<IviSurface>(
      shared_from_this(), ivi_id, width, height, buffer_count, buffer_format,
      frame_callback, egl_config);
  return ivi_surface_;
}

#endif  // ENABLE_IVI_SHELL_CLIENT
