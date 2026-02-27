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
#include "waypp/window/window.h"

class IviWindowManager;

/**
 * @class IviSurface
 *
 * @brief A Wayland surface registered with the IVI compositor via
 *        ivi_application_surface_create().
 *
 * IviSurface inherits from Window and mirrors the role that XdgTopLevel
 * plays in the XDG-shell path. On construction, it calls
 * ivi_application_surface_create() to obtain an ivi_surface* handle,
 * registers the configure listener, and blocks until the compositor sends
 * the first configure event (identical to XdgTopLevel's wait_for_configure_
 * pattern).
 *
 * The caller-supplied IVI surface ID must be unique across all IVI surfaces
 * visible to the compositor. Applications should allocate IDs from a
 * system-wide manifest or registry.
 */
class IviSurface : public Window {
 public:
  /**
   * @brief Construct and register an IVI surface.
   *
   * @param window_manager   The owning IviWindowManager (kept alive via
   *                         shared_ptr).
   * @param ivi_id           IVI surface ID (must be unique on this display).
   * @param width            Initial surface width in pixels.
   * @param height           Initial surface height in pixels.
   * @param buffer_count     Number of backing buffers.
   * @param buffer_format    wl_shm pixel format.
   * @param frame_callback   Invoked each frame with (user_data, timestamp_ms).
   * @param egl_config       EGL configuration, or nullptr for SHM rendering.
   */
  IviSurface(std::shared_ptr<IviWindowManager> window_manager,
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

  ~IviSurface();

  /// Returns the IVI surface ID this surface was created with.
  [[nodiscard]] uint32_t get_ivi_id() const { return ivi_id_; }

  // Disallow copy and assign.
  IviSurface(const IviSurface&) = delete;

  IviSurface& operator=(const IviSurface&) = delete;

 private:
  /// Raw IVI surface handle (owned by this object).
  ivi_surface* ivi_surface_{};

  /// IVI surface ID supplied at construction time.
  uint32_t ivi_id_;

  /// Blocks the constructor until the compositor sends the first configure.
  volatile bool wait_for_configure_{true};

  /// Owning window manager (kept alive for the lifetime of this surface).
  std::shared_ptr<IviWindowManager> window_manager_;

  static const ivi_surface_listener ivi_surface_listener_;

  /**
   * @brief Compositor-pushed configure event handler.
   *
   * Updates the surface dimensions, commits the surface, and clears
   * wait_for_configure_ so the constructor can proceed.
   */
  static void handle_configure(void* data,
                               ivi_surface* surface,
                               int32_t width,
                               int32_t height);
};

#endif  // ENABLE_IVI_SHELL_CLIENT
