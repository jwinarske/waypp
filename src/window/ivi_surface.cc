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

#include "waypp/window/ivi_surface.h"

#if ENABLE_IVI_SHELL_CLIENT

#include <stdexcept>

#include "logging/logging.h"
#include "waypp/window_manager/ivi_window_manager.h"

/**
 * @brief Static ivi_surface listener vtable.
 *
 * Only one event is defined by the ivi_application protocol: configure.
 * All other lifecycle management (destroy) is handled by the destructor.
 */
const ivi_surface_listener IviSurface::ivi_surface_listener_ = {
    .configure = handle_configure,
};

/**
 * @class IviSurface
 *
 * @brief Registers a wl_surface with the IVI compositor via
 *        ivi_application_surface_create().
 *
 * Construction flow:
 *  1. Initialise the Window base class (creates wl_surface, allocates buffers).
 *  2. Record initial dimensions via set_width/set_height.
 *  3. Call ivi_application_surface_create() to obtain an ivi_surface* handle.
 *  4. Register the configure listener.
 *  5. Commit the surface and block on wl_display_dispatch() until the
 *     compositor sends the first configure event, identical to XdgTopLevel's
 *     wait_for_configure_ pattern.
 */
IviSurface::IviSurface(
    std::shared_ptr<IviWindowManager> window_manager,
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
    )
    : Window(window_manager,
             "ivi-surface",
             buffer_count,
             buffer_format,
             frame_callback,
             width,
             height,
             /*fullscreen=*/false,
             /*maximized=*/false,
             /*fullscreen_ratio=*/false,
             /*tearing=*/false,
#if ENABLE_EGL
             egl_config
#else
             nullptr
#endif
             ),
      ivi_id_(ivi_id),
      window_manager_(std::move(window_manager)) {
  DLOG_TRACE("++IviSurface::IviSurface() ivi_id={}", ivi_id_);

  set_width(width);
  set_height(height);
  set_init_width(width);
  set_init_height(height);

  const auto ivi_application = window_manager_->get_ivi_application();
  if (!ivi_application) {
    throw std::runtime_error(
        "ivi_application is not available; cannot create IviSurface");
  }

  const auto surface = get_surface();

  ivi_surface_ =
      ivi_application_surface_create(ivi_application, ivi_id_, surface);
  if (!ivi_surface_) {
    throw std::runtime_error(
        "ivi_application_surface_create() failed — ivi_id may already be in "
        "use or the wl_surface already has a role");
  }

  ivi_surface_add_listener(ivi_surface_, &ivi_surface_listener_, this);

  wait_for_configure_ = true;
  wl_surface_commit(surface);

  // Block until the compositor sends the first configure event, which provides
  // the authoritative surface dimensions.  This mirrors XdgTopLevel's startup
  // synchronisation pattern.
  while (wait_for_configure_) {
    wl_display_dispatch(window_manager_->get_display());
  }

  DLOG_TRACE("--IviSurface::IviSurface() ivi_id={} size={}x{}", ivi_id_,
             get_width(), get_height());
}

IviSurface::~IviSurface() {
  if (ivi_surface_) {
    DLOG_TRACE("[IviSurface] ivi_surface_destroy() ivi_id={}", ivi_id_);
    ivi_surface_destroy(ivi_surface_);
    ivi_surface_ = nullptr;
  }
}

/**
 * @brief Compositor-pushed configure event handler.
 *
 * Called by the compositor to suggest a surface size.  If the compositor
 * supplies non-zero dimensions, they are adopted; otherwise the dimensions
 * recorded at construction time are retained.  The surface is committed to
 * acknowledge the configure and wait_for_configure_ is cleared so the
 * constructor's dispatch loop can exit.
 *
 * @param data    Pointer to the owning IviSurface instance.
 * @param surface The ivi_surface that triggered the event.
 * @param width   Suggested width in surface-local coordinates (0 = no hint).
 * @param height  Suggested height in surface-local coordinates (0 = no hint).
 */
void IviSurface::handle_configure(void* data,
                                  ivi_surface* surface,
                                  const int32_t width,
                                  const int32_t height) {
  auto* self = static_cast<IviSurface*>(data);
  if (self->ivi_surface_ != surface) {
    return;
  }

  DLOG_TRACE("[IviSurface] handle_configure ivi_id={} size={}x{}",
             self->ivi_id_, width, height);

  if (width > 0 && height > 0) {
    self->set_width(width);
    self->set_height(height);
  }

  self->set_needs_buffer_geometry_update(true);

  wl_surface_commit(self->get_surface());
  self->wait_for_configure_ = false;
}

#endif  // ENABLE_IVI_SHELL_CLIENT

