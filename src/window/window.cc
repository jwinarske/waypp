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

#include "waypp/window/window.h"

#include <wayland-egl.h>
#include <algorithm>

#include "logging/logging.h"
using waypp::Egl;

Window::Window(std::shared_ptr<WindowManager> wm,
               const char* name,
               const int buffer_count,
               uint32_t buffer_format,
               const std::function<void(void*, uint32_t)>& frame_callback,
               int width,
               int height,
               const bool fullscreen,
               const bool maximized,
               const bool fullscreen_ratio,
               const bool tearing,
               Egl::config* egl_config)
    : wm_(std::move(wm)),
      outputs_(wm_->get_outputs()),
      buffer_transform_(WL_OUTPUT_TRANSFORM_NORMAL),
      runtime_mode_(WINDOW_RUNTIME_MODE_FEEDBACK),
      name_(name),
      frame_callback_(frame_callback),
      fullscreen_(fullscreen),
      maximized_(maximized),
      fullscreen_ratio_(fullscreen_ratio),
      valid_(true),
      buffer_count_(buffer_count),
      buffer_format_(buffer_format),
      extents_({
          .init = {.width = width, .height = height},
          .max = {.width = INT32_MAX, .height = INT32_MAX},
          .buffer = {.width = width, .height = height},
          .window = {.width = width, .height = height},
          .logical = {.width = width, .height = height},
      }),
      needs_buffer_geometry_update_(false) {
  wl_surface_ = wl_compositor_create_surface(wm_->get_compositor());
  if (!wl_surface_) {
    throw std::runtime_error("failed to create Wayland surface");
  }
  wl_surface_add_listener(wl_surface_, &surface_listener_, this);

  if (buffer_count) {
    if (!wm_->shm_has_format(static_cast<wl_shm_format>(buffer_format))) {
      throw std::runtime_error(std::string("SHM format not supported: ") +
                               WindowManager::shm_format_to_text(
                                   static_cast<wl_shm_format>(buffer_format)));
    }
    buffers_.reserve(static_cast<unsigned long>(buffer_count));
    for (int i = 0; i < buffer_count_; i++) {
      auto buffer = std::make_unique<Buffer>(wm_->get_shm());
      if (buffer->create_shm_buffer(width, height, buffer_format_) < 0) {
        throw std::runtime_error("failed to create SHM buffer");
      }
      buffers_.push_back(std::move(buffer));
    }
  }

  if (runtime_mode_ == WINDOW_RUNTIME_MODE_PRESENTATION) {
    presentation_.wp_presentation =
        wm_->get_presentation_time_wp_presentation();
    presentation_.clock_id = wm_->get_presentation_time_clk_id();
  }

#if ENABLE_EGL
  if (egl_config && egl_config->context_attribs_size &&
      egl_config->config_attribs_size) {
    egl_ = std::make_unique<waypp::Egl>(wm_->get_display(), wl_surface_, width,
                                        height, egl_config);
    egl_->set_swap_interval(egl_config->swap_interval);
  }
#endif

  if (wm_->get_viewporter()) {
#if HAS_WAYLAND_PROTOCOL_VIEWPORTER
    viewport_ = wl_proxy_marshal_constructor(
        wm_->get_viewporter(),
        viewporter::client::wp_viewporter_traits::Op::GetViewport,
        &viewporter::client::wp_viewport_traits::wl_iface(),
        nullptr, (wl_proxy*)wl_surface_);
    if (!viewport_) {
      LOG_WARN("failed to get Wayland viewport");
    }
#endif
  }

#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1
  if (wm_->get_fractional_scale_manager()) {
    fractional_scale_ = wl_proxy_marshal_constructor(
        wm_->get_fractional_scale_manager(),
        fractional_scale_v1::client::wp_fractional_scale_manager_v1_traits::Op::GetFractionalScale,
        &fractional_scale_v1::client::wp_fractional_scale_v1_traits::wl_iface(),
        nullptr, (wl_proxy*)wl_surface_);
    if (fractional_scale_) {
      wl_proxy_add_listener(fractional_scale_,
                            reinterpret_cast<void(**)(void)>(
                                const_cast<void**>(fractional_scale_listener_)),
                            this);
    } else {
      LOG_WARN("failed to get Wayland fractional scale");
    }
  }
#endif

#if HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1
  if (wm_->get_tearing_control_manager()) {
    tearing_control_ = wl_proxy_marshal_constructor(
        wm_->get_tearing_control_manager(),
        tearing_control_v1::client::wp_tearing_control_manager_v1_traits::Op::GetTearingControl,
        &tearing_control_v1::client::wp_tearing_control_v1_traits::wl_iface(),
        nullptr, (wl_proxy*)wl_surface_);
    if (tearing_control_) {
      if (tearing) {
        DLOG_DEBUG("[Surface] Set Presentation Hint: ASYNC");
        wl_proxy_marshal(tearing_control_,
                         tearing_control_v1::client::wp_tearing_control_v1_traits::Op::SetPresentationHint,
                         static_cast<uint32_t>(tearing_control_v1::client::WpTearingControlV1PresentationHint::Async));
      } else {
        DLOG_DEBUG("[Surface] Set Presentation Hint: VSYNC");
        wl_proxy_marshal(tearing_control_,
                         tearing_control_v1::client::wp_tearing_control_v1_traits::Op::SetPresentationHint,
                         static_cast<uint32_t>(tearing_control_v1::client::WpTearingControlV1PresentationHint::Vsync));
      }
    } else {
      LOG_WARN("failed to get Wayland tearing control");
    }
  }
#else
  (void)tearing;
#endif
}

Window::~Window() {
  if (viewport_) {
#if HAS_WAYLAND_PROTOCOL_VIEWPORTER
    DLOG_TRACE("[Window] wl_proxy_destroy(viewport_)");
    wl_proxy_destroy(viewport_);
#endif
  }
  if (fractional_scale_) {
#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1
    DLOG_TRACE("[Window] wl_proxy_destroy(fractional_scale_)");
    wl_proxy_destroy(fractional_scale_);
#endif
  }
  if (wl_callback_) {
    DLOG_TRACE("[Window] wl_callback_destroy(wl_callback_)");
    wl_callback_destroy(wl_callback_);
  }
  if (wl_surface_) {
    DLOG_TRACE("[Window] wl_surface_destroy(wl_surface_)");
    wl_surface_destroy(wl_surface_);
  }

  if (buffer_count_) {
    for (auto& buffer : buffers_) {
      buffer.reset();
    }
  }
}

void Window::update_buffer_geometry() {
  if (!needs_buffer_geometry_update_) {
    return;
  }

  DLOG_DEBUG("update_buffer_geometry");
  struct {
    int width;
    int height;
  } new_buffer_size{};
  struct {
    int width;
    int height;
  } new_viewport_dest_size{};

  auto new_buffer_transform = wm_->get_output_buffer_transform(wl_output_);
  if (buffer_transform_ != new_buffer_transform) {
    buffer_transform_ = new_buffer_transform;
    wl_surface_set_buffer_transform(wl_surface_, buffer_transform_);
  }

  switch (buffer_transform_) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_180:
    case WL_OUTPUT_TRANSFORM_FLIPPED:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
      new_buffer_size.width = extents_.logical.width;
      new_buffer_size.height = extents_.logical.height;
      break;
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      new_buffer_size.width = extents_.logical.height;
      new_buffer_size.height = extents_.logical.width;
      break;
  }

  buffer_scale_ = wm_->get_output_buffer_scale(wl_output_);

  if (fractional_buffer_scale_ > 0.0) {
    if (buffer_scale_ > 1) {
      buffer_scale_ = 1;
      wl_surface_set_buffer_scale(wl_surface_, buffer_scale_);
    }

    new_buffer_size.width = static_cast<int>(
        ceil(new_buffer_size.width * fractional_buffer_scale_));
    new_buffer_size.height = static_cast<int>(
        ceil(new_buffer_size.height * fractional_buffer_scale_));
  } else {
    if (const int32_t new_buffer_scale =
            wm_->get_output_buffer_scale(wl_output_);
        buffer_scale_ != new_buffer_scale) {
      buffer_scale_ = new_buffer_scale;
      wl_surface_set_buffer_scale(wl_surface_, buffer_scale_);
    }

    new_buffer_size.width *= buffer_scale_;
    new_buffer_size.height *= buffer_scale_;
  }

  if (fullscreen_ && fullscreen_ratio_) {
    const int new_buffer_size_min =
        std::min(new_buffer_size.width, new_buffer_size.height);
    new_buffer_size.width = new_buffer_size_min;
    new_buffer_size.height = new_buffer_size_min;

    const int new_viewport_dest_size_min =
        std::min(extents_.logical.width, extents_.logical.height);
    new_viewport_dest_size.width = new_viewport_dest_size_min;
    new_viewport_dest_size.height = new_viewport_dest_size_min;
  } else {
    new_viewport_dest_size.width = extents_.logical.width;
    new_viewport_dest_size.height = extents_.logical.height;
  }

  if (extents_.buffer.width != new_buffer_size.width ||
      extents_.buffer.height != new_buffer_size.height) {
    extents_.buffer.width = new_buffer_size.width;
    extents_.buffer.height = new_buffer_size.height;
#if ENABLE_EGL
    if (egl_) {
      LOG_DEBUG("egl_->resize({}, {}, 0, 0)", extents_.buffer.width,
                extents_.buffer.height);
      egl_->resize(extents_.buffer.width, extents_.buffer.height, 0, 0);
    }
#endif
  }

  if (fractional_buffer_scale_ > 0.0) {
    if (viewport_) {
      wl_proxy_marshal(viewport_,
                       viewporter::client::wp_viewport_traits::Op::SetDestination,
                       new_viewport_dest_size.width,
                       new_viewport_dest_size.height);
    }
  }

  // Keep extents_.window in sync so next_buffer() recreates SHM buffers at
  // the correct size after a compositor-driven resize (LOW-8 / resize fix).
  extents_.window.width = new_buffer_size.width;
  extents_.window.height = new_buffer_size.height;

  needs_buffer_geometry_update_ = false;
}

void Window::handle_preferred_scale(
    void* data,
    wl_proxy* /*wp_fractional_scale_v1*/,
    const uint32_t scale) {
  LOG_TRACE("[Window] handle_preferred_scale()");
  auto* w = static_cast<Window*>(data);
  w->fractional_buffer_scale_ = static_cast<double>(scale) / 120;
  w->needs_buffer_geometry_update_ = true;
}

void Window::handle_surface_enter(void* data,
                                  wl_surface* wl_surface,
                                  wl_output* wl_output) {
  const auto obj = static_cast<Window*>(data);
  if (obj->wl_surface_ != wl_surface) {
    return;
  }
  LOG_TRACE("handle_surface_enter: {} [{}]", fmt::ptr(wl_output), obj->name_);
  obj->wl_output_ = wl_output;
}

void Window::handle_surface_leave(void* data,
                                  wl_surface* wl_surface,
                                  wl_output* output) {
  const auto obj = static_cast<Window*>(data);
  if (obj->wl_surface_ != wl_surface) {
    return;
  }
#if !defined(NDEBUG)
  LOG_TRACE("handle_surface_leave: {} [{}]", fmt::ptr(output), obj->name_);
#else
  (void)output;
#endif
  obj->wl_output_ = nullptr;
}

/**
 * @brief Start rendering frames for the surface.
 *
 * This function stops the current frames (if any) and starts rendering new
 * frames by calling the `on_frame` function.
 *
 * @note This function assumes that the surface has been initialized properly.
 */
void Window::start_frame_callbacks(void* user_data) {
  LOG_TRACE("[Window] start_frame_callbacks");
  if (user_data) {
    user_data_ = user_data;
  }
  handle_frame_callback(this, nullptr, 0);
}

/**
 * Stops the frame rendering by destroying the wl_callback object if it exists.
 * This function is intended to be called from outside the Window class.
 */
void Window::stop_frame_callbacks() {
  LOG_TRACE("[Window] stop_frame_callbacks");
  if (wl_callback_) {
    LOG_TRACE("[Window] wl_callback_destroy");
    wl_callback_destroy(wl_callback_);
    wl_callback_ = nullptr;
  }
}

/**
 * @brief Callback function for frame completion event
 *
 * This function is called when a frame completion event is received.
 * It updates the state of the Window object and invokes the
 * draw_frame_callback_ function.
 *
 * @param data Pointer to the Surface object
 * @param callback Pointer to the wl_callback object
 * @param time Timestamp of the frame completion event
 */
void Window::handle_frame_callback(void* data,
                                   wl_callback* callback,
                                   const uint32_t time) {
  // LOG_TRACE("++Window::handle_frame_callback()");
  const auto obj = static_cast<Window*>(data);

  obj->wl_callback_ = nullptr;

  if (callback) {
    wl_callback_destroy(callback);
  }

  if (obj->frame_callback_) {
    obj->frame_callback_(obj->user_data_ ? obj->user_data_ : data, time);
  }

  if (obj->wl_surface_) {
    obj->wl_callback_ = wl_surface_frame(obj->wl_surface_);
    wl_callback_add_listener(obj->wl_callback_,
                             &Window::frame_callback_listener_, data);

    if (obj->runtime_mode_ == WINDOW_RUNTIME_MODE_PRESENTATION) {
      // struct wp_presentation *wp_presentation, clockid_t clock_id, struct
      // wl_surface *wl_surface, uint32_t time, FeedbackObserver *observer =
      // nullptr
      auto feedback = std::make_unique<Feedback>(
          obj->presentation_.wp_presentation, obj->presentation_.clock_id,
          obj->wl_surface_, time);

      // Register a completion hook so the entry is removed from feedback_list
      // when the compositor sends presented or discarded, preventing unbounded
      // growth of the list for long-running sessions (LOW-8).
      Feedback* raw = feedback.get();
      raw->set_on_done([obj](Feedback* done) {
        auto& list = obj->presentation_.feedback_list;
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [done](const std::unique_ptr<Feedback>& p) {
                                    return p.get() == done;
                                  }),
                   list.end());
      });

      obj->presentation_.feedback_list.push_back(std::move(feedback));
      //            window_create_feedback(window, time);
      //            window_commit_next(window);
    }
    wl_surface_commit(obj->wl_surface_);
  }
  // LOG_TRACE("--Window::handle_frame_callback()");
}

void Window::handle_preferred_buffer_scale(void* data,
                                           wl_surface* wl_surface,
                                           const int32_t factor) {
  LOG_DEBUG("[Window] handle_preferred_buffer_scale()");
  const auto w = static_cast<Window*>(data);
  if (w->wl_surface_ != wl_surface) {
    return;
  }
  w->preferred_buffer_scale_ = factor;
  w->needs_buffer_geometry_update_ = true;
}

void Window::handle_preferred_buffer_transform(void* data,
                                               wl_surface* wl_surface,
                                               uint32_t transform) {
  LOG_DEBUG("[Window] handle_preferred_buffer_transform()");
  const auto w = static_cast<Window*>(data);
  if (w->wl_surface_ != wl_surface) {
    return;
  }
  w->preferred_buffer_transform_ = static_cast<wl_output_transform>(transform);
}

void Window::resize(const int width, const int height) {
#if ENABLE_EGL
  if (egl_) {
    extents_.logical.width = width;
    extents_.logical.height = height;
    egl_->resize(width, height, 0, 0);
  }
#endif
}

void Window::make_current() const {
#if ENABLE_EGL
  if (egl_) {
    egl_->make_current();
  }
#endif
}

void Window::clear_current() const {
#if ENABLE_EGL
  if (egl_) {
    egl_->clear_current();
  }
#endif
}

void Window::swap_buffers() const {
#if ENABLE_EGL
  if (egl_) {
    egl_->swap_buffers();
  }
#endif
}

bool Window::have_swap_buffers_width_damage() const {
#if ENABLE_EGL
  if (egl_) {
    return egl_->have_swap_buffers_with_damage();
  }
#endif
  return false;
}

void Window::get_buffer_age(EGLint& buffer_age) const {
#if ENABLE_EGL
  if (egl_) {
    egl_->get_buffer_age(buffer_age);
  }
#endif
}

void Window::swap_buffers_with_damage(EGLint* rects, EGLint n_rects) const {
#if ENABLE_EGL
  if (egl_) {
    egl_->swap_buffers_with_damage(rects, n_rects);
  }
#endif
}

Buffer* Window::pick_free_buffer() const {
  Buffer* res = nullptr;
  for (auto& b : buffers_) {
    if (!b->is_busy()) {
      res = b.get();
      break;
    }
  }
  return res;
}

Buffer* Window::next_buffer() const {
  const auto buffer = pick_free_buffer();

  if (!buffer)
    return nullptr;

  // Recreate the SHM buffer if it has never been allocated or if the window
  // has been resized since the buffer was last created.
  const bool size_changed = buffer->get_wl_buffer() &&
                            (buffer->get_width() != extents_.window.width ||
                             buffer->get_height() != extents_.window.height);

  if (!buffer->get_wl_buffer() || size_changed) {
    if (size_changed) {
      buffer->destroy();
    }
    const auto ret = buffer->create_shm_buffer(
        extents_.window.width, extents_.window.height, buffer_format_);

    if (ret < 0)
      return nullptr;

    /* paint the padding */
    memset(buffer->get_shm_data(), 0xff, buffer->get_size());
  }

  return buffer;
}

void Window::opaque_region_add(const int32_t x,
                               const int32_t y,
                               const int32_t width,
                               const int32_t height) {
  const auto region = wl_compositor_create_region(wm_->get_compositor());
  wl_region_add(region, x, y, width, height);
  wl_surface_set_opaque_region(wl_surface_, region);
  LOG_TRACE("[Window] wl_region_destroy(region)");
  wl_region_destroy(region);
}

void Window::opaque_region_clear() const {
  wl_surface_set_opaque_region(wl_surface_, nullptr);
}

void Window::presentation_feedback_add_callbacks() {}
