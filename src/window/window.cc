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

#include "window.h"

#include <wayland-egl.h>

#include "logging.h"

Window::Window(WindowManager *wm,
               const char *name,
               int buffer_count,
               uint32_t buffer_format,
               const std::function<void(void *, const uint32_t)> &frame_callback,
               int width,
               int height,
               bool fullscreen,
               bool maximized,
               bool fullscreen_ratio,
               bool tearing,
               Egl::config *egl_config)
        : wm_(wm),
          buffer_transform_(WL_OUTPUT_TRANSFORM_NORMAL),
          frame_callback_(frame_callback),
          name_(name),
          fullscreen_(fullscreen),
          maximized_(maximized),
          fullscreen_ratio_(fullscreen_ratio),
          runtime_mode_(WINDOW_RUNTIME_MODE_FEEDBACK),
          outputs_(wm->get_outputs()),
          valid_(true),
          logical_size_{.width = width, .height = height},
          buffer_size_{.width = width, .height = height},
          window_size_{.width = buffer_size_.width, .height = buffer_size_.height},
          needs_buffer_geometry_update_(false),
          buffer_count_(buffer_count),
          buffer_format_(buffer_format) {
    wl_surface_ = wl_compositor_create_surface(wm->get_compositor());
    wl_surface_add_listener(wl_surface_, &surface_listener_, this);

    if (buffer_count) {
        if (!wm->shm_has_format(static_cast<wl_shm_format>(buffer_format))) {
            spdlog::critical("{} is not supported.",
                             WindowManager::shm_format_to_text(
                                     static_cast<wl_shm_format>(buffer_format)));
            abort();
        }
        buffers_.reserve(static_cast<unsigned long>(buffer_count));
        for (int i = 0; i < buffer_count_; i++) {
            auto buffer = std::make_unique<Buffer>(wm_->get_shm());
            buffer->create_shm_buffer(width, height, buffer_format_);
            buffers_.push_back(std::move(buffer));
        }
    }

    if (runtime_mode_ == WINDOW_RUNTIME_MODE_PRESENTATION) {
        presentation_.wp_presentation =
                wm_->get_presentation_time_wp_presentation();
        presentation_.clock_id = wm_->get_presentation_time_clk_id();
    }

#if ENABLE_EGL
    if (egl_config && egl_config->context_attribs_size && egl_config->config_attribs_size) {
        egl_ = std::make_unique<Egl>(wm->get_display(), wl_surface_, width, height, egl_config);
        egl_->set_swap_interval(egl_config->swap_interval);
    }
#endif

    if (wm->get_viewporter()) {
#if HAS_WAYLAND_PROTOCOL_VIEWPORTER
        viewport_ = wp_viewporter_get_viewport(wm->get_viewporter(), wl_surface_);
#endif
    }

    if (wm->get_fractional_scale_manager()) {
#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1
        fractional_scale_ = wp_fractional_scale_manager_v1_get_fractional_scale(
                wm->get_fractional_scale_manager(), wl_surface_);
        wp_fractional_scale_v1_add_listener(fractional_scale_,
                                            &fractional_scale_listener_, this);
#endif
    }

    if (wm->get_tearing_control_manager()) {
#if HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1
        tearing_control_ = wp_tearing_control_manager_v1_get_tearing_control(
                wm->get_tearing_control_manager(), wl_surface_);
        if (tearing) {
            SPDLOG_DEBUG("[Surface] Set Presentation Hint: ASYNC");
            wp_tearing_control_v1_set_presentation_hint(
                    tearing_control_, WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC);
        } else {
            SPDLOG_DEBUG("[Surface] Set Presentation Hint: VSYNC");
            wp_tearing_control_v1_set_presentation_hint(
                    tearing_control_, WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC);
        }
#else
        (void)tearing;
#endif
    }
}

Window::~Window() {
    if (viewport_) {
#if HAS_WAYLAND_PROTOCOL_VIEWPORTER
        wp_viewport_destroy(viewport_);
#endif
    }
    if (fractional_scale_) {
#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1
        wp_fractional_scale_v1_destroy(fractional_scale_);
#endif
    }
    if (wl_callback_) {
        wl_callback_destroy(wl_callback_);
    }
    if (wl_surface_) {
        wl_surface_destroy(wl_surface_);
    }

    if (buffer_count_) {
        for (auto &buffer: buffers_) {
            buffer.reset();
        }
    }
}

void Window::update_buffer_geometry() {
    if (!needs_buffer_geometry_update_) {
        return;
    }

    enum wl_output_transform new_buffer_transform;
    struct {
        int width;
        int height;
    } new_buffer_size{};
    struct {
        int width;
        int height;
    } new_viewport_dest_size{};

    new_buffer_transform = wm_->get_output_buffer_transform(wl_output_);
    if (buffer_transform_ != new_buffer_transform) {
        buffer_transform_ = new_buffer_transform;
        wl_surface_set_buffer_transform(wl_surface_, buffer_transform_);
    }

    switch (buffer_transform_) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
        case WL_OUTPUT_TRANSFORM_180:
        case WL_OUTPUT_TRANSFORM_FLIPPED:
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            new_buffer_size.width = logical_size_.width;
            new_buffer_size.height = logical_size_.height;
            break;
        case WL_OUTPUT_TRANSFORM_90:
        case WL_OUTPUT_TRANSFORM_270:
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            new_buffer_size.width = logical_size_.height;
            new_buffer_size.height = logical_size_.width;
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
        int32_t new_buffer_scale;

        new_buffer_scale = wm_->get_output_buffer_scale(wl_output_);
        if (buffer_scale_ != new_buffer_scale) {
            buffer_scale_ = new_buffer_scale;
            wl_surface_set_buffer_scale(wl_surface_, buffer_scale_);
        }

        new_buffer_size.width *= buffer_scale_;
        new_buffer_size.height *= buffer_scale_;
    }

    if (fullscreen_ && fullscreen_ratio_) {
        int new_buffer_size_min;
        int new_viewport_dest_size_min;

        new_buffer_size_min =
                std::min(new_buffer_size.width, new_buffer_size.height);
        new_buffer_size.width = new_buffer_size_min;
        new_buffer_size.height = new_buffer_size_min;

        new_viewport_dest_size_min =
                std::min(logical_size_.width, logical_size_.height);
        new_viewport_dest_size.width = new_viewport_dest_size_min;
        new_viewport_dest_size.height = new_viewport_dest_size_min;
    } else {
        new_viewport_dest_size.width = logical_size_.width;
        new_viewport_dest_size.height = logical_size_.height;
    }

    if (buffer_size_.width != new_buffer_size.width ||
        buffer_size_.height != new_buffer_size.height) {
        buffer_size_.width = new_buffer_size.width;
        buffer_size_.height = new_buffer_size.height;
#if ENABLE_EGL
        if (egl_) {
            egl_->resize(buffer_size_.width, buffer_size_.height, 0, 0);
        }
#endif
    }

    if (fractional_buffer_scale_ > 0.0) {
        if (viewport_) {
            wp_viewport_set_destination(viewport_, new_viewport_dest_size.width,
                                        new_viewport_dest_size.height);
        }
    }

    needs_buffer_geometry_update_ = false;
}

void Window::handle_preferred_scale(
        void *data,
        struct wp_fractional_scale_v1 *wp_fractional_scale_v1,
        uint32_t scale) {
    SPDLOG_TRACE("[Window] handle_preferred_scale()");
    auto *w = static_cast<Window *>(data);
    if (w->fractional_scale_ != wp_fractional_scale_v1) {
        return;
    }
    w->fractional_buffer_scale_ = static_cast<double>(scale) / 120;
    w->needs_buffer_geometry_update_ = true;
}

void Window::handle_surface_enter(void *data,
                                  struct wl_surface *wl_surface,
                                  struct wl_output *wl_output) {
    auto obj = static_cast<Window *>(data);
    if (obj->wl_surface_ != wl_surface) {
        return;
    }
    SPDLOG_TRACE("handle_surface_enter: {} [{}]", fmt::ptr(wl_output),
                 obj->name_);
    obj->wl_output_ = wl_output;
}

void Window::handle_surface_leave(void *data,
                                  struct wl_surface *wl_surface,
                                  struct wl_output *output) {
    auto obj = static_cast<Window *>(data);
    if (obj->wl_surface_ != wl_surface) {
        return;
    }
#if !defined(NDEBUG)
    SPDLOG_TRACE("handle_surface_leave: {} [{}]", fmt::ptr(output), obj->name_);
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
void Window::start_frame_callbacks(void *user_data) {
    SPDLOG_TRACE("[Window] start_frame_callbacks");
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
    SPDLOG_TRACE("[Window] stop_frame_callbacks");
    if (wl_callback_) {
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
void Window::handle_frame_callback(void *data,
                                   struct wl_callback *callback,
                                   const uint32_t time) {
    SPDLOG_TRACE("++Window::handle_frame_callback()");
    const auto obj = static_cast<Window *>(data);

    obj->wl_callback_ = nullptr;

    if (callback) {
        wl_callback_destroy(callback);
    }

    if (obj->frame_callback_) {
        obj->frame_callback_(data, time);
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
            obj->presentation_.feedback_list.push_back(std::move(feedback));
            //            window_create_feedback(window, time);
            //            window_commit_next(window);
        }
        wl_surface_commit(obj->wl_surface_);
    }
    SPDLOG_TRACE("--Window::handle_frame_callback()");
}

void Window::handle_preferred_buffer_scale(void *data,
                                           struct wl_surface *wl_surface,
                                           int32_t factor) {
    SPDLOG_DEBUG("[Window] handle_preferred_buffer_scale()");
    auto w = static_cast<Window *>(data);
    if (w->wl_surface_ != wl_surface) {
        return;
    }
    w->preferred_buffer_scale_ = factor;
    w->needs_buffer_geometry_update_ = true;
}

void Window::handle_preferred_buffer_transform(void *data,
                                               struct wl_surface *wl_surface,
                                               uint32_t transform) {
    SPDLOG_DEBUG("[Window] handle_preferred_buffer_transform()");
    auto w = static_cast<Window *>(data);
    if (w->wl_surface_ != wl_surface) {
        return;
    }
    w->preferred_buffer_transform_ =
            static_cast<enum wl_output_transform>(transform);
}

void Window::resize(int width, int height) {
#if ENABLE_EGL
    if (egl_) {
        logical_size_.width = width;
        logical_size_.height = height;
        egl_->resize(width, height, 0, 0);
    }
#endif
}

void Window::make_current() {
#if ENABLE_EGL
    if (egl_) {
        egl_->make_current();
    }
#endif
}

void Window::clear_current() {
#if ENABLE_EGL
    if (egl_) {
        egl_->clear_current();
    }
#endif
}

void Window::swap_buffers() {
#if ENABLE_EGL
    if (egl_) {
        egl_->swap_buffers();
    }
#endif
}

bool Window::have_swap_buffers_width_damage() {
#if ENABLE_EGL
    if (egl_) {
        return egl_->have_swap_buffers_with_damage();
    }
#endif
    return false;
}

void Window::get_buffer_age(EGLint &buffer_age) {
#if ENABLE_EGL
    if (egl_) {
        egl_->get_buffer_age(buffer_age);
    }
#endif
}

void Window::swap_buffers_with_damage(const EGLint *rects, EGLint n_rects) {
#if ENABLE_EGL
    if (egl_) {
        egl_->swap_buffers_with_damage(rects, n_rects);
    }
#endif
}

Buffer *Window::pick_free_buffer() {
    Buffer *res = nullptr;
    for (auto &b: buffers_) {
        if (!b->is_busy()) {
            res = b.get();
            break;
        }
    }
    return res;
}

Buffer *Window::next_buffer() {
    auto buffer = pick_free_buffer();

    if (!buffer)
        return nullptr;

    if (!buffer->get_wl_buffer()) {
        auto ret = buffer->create_shm_buffer(window_size_.width,
                                             window_size_.height, buffer_format_);

        if (ret < 0)
            return nullptr;

        /* paint the padding */
        memset(buffer->get_shm_data(), 0xff,
               static_cast<size_t>(window_size_.width) *
               static_cast<size_t>(window_size_.height) * 4);
    }

    return buffer;
}

void Window::opaque_region_add(int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height) {
    auto region = wl_compositor_create_region(wm_->get_compositor());
    wl_region_add(region, x, y, width, height);
    wl_surface_set_opaque_region(wl_surface_, region);
    wl_region_destroy(region);
}

void Window::opaque_region_clear() {
    wl_surface_set_opaque_region(wl_surface_, nullptr);
}

void Window::presentation_feedback_add_callbacks() {}
