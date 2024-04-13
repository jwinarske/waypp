
#include "window.h"

#include <wayland-egl.h>

#include "logging.h"

Window::Window(WindowManager *wm, struct wl_compositor *wl_compositor,
               const std::optional<struct wp_viewporter *> &wl_viewporter,
               const std::optional<struct wp_fractional_scale_manager_v1 *> &fractional_scale_manager,
               const std::optional<struct wp_tearing_control_manager_v1 *> &tearing_control_manager,
               const std::map<struct wl_output *, std::unique_ptr<Output>> &outputs,
               const char *name, const std::function<void(void *, const uint32_t)> &draw_frame_callback,
               int width, int height, wl_output_transform buffer_transform, bool fullscreen,
               bool maximized, bool fullscreen_ratio, bool tearing,
               const int32_t *context_attribs, size_t context_attribs_size,
               const int32_t *config_attribs, size_t config_attribs_size,
               int buffer_bpp, int swap_interval) :
        wm_(wm), buffer_transform_(buffer_transform), draw_frame_callback_(draw_frame_callback),
        name_(name), fullscreen_(fullscreen), maximized_(maximized), fullscreen_ratio_(fullscreen_ratio),
        outputs_(outputs), logical_size_{.width=width, .height=height},
        buffer_bpp_(buffer_bpp), swap_interval_(swap_interval), buffer_size_{.width=width, .height=height},
        window_size_{.width=buffer_size_.width, .height=buffer_size_.height}, needs_buffer_geometry_update_(false) {

    wl_surface_ = wl_compositor_create_surface(wl_compositor);
    wl_surface_add_listener(wl_surface_, &surface_listener_, this);

    if (context_attribs_size && config_attribs_size) {

        egl_ = std::make_unique<Egl>(wm->get_display(), wl_surface_, width, height, context_attribs,
                                     context_attribs_size, config_attribs, config_attribs_size, buffer_bpp);
        egl_->set_swap_interval(swap_interval);
    }

    if (wl_viewporter.has_value()) {
#if defined(WAYLAND_PROTOCOL_HAS_VIEWPORTER)
        viewport_ = wp_viewporter_get_viewport(wl_viewporter.value(), wl_surface_);
#endif
    }

    if (fractional_scale_manager.has_value()) {
#if defined(WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE)
        fractional_scale_ = wp_fractional_scale_manager_v1_get_fractional_scale(fractional_scale_manager.value(),
                                                                                wl_surface_);
        wp_fractional_scale_v1_add_listener(fractional_scale_, &fractional_scale_listener_, this);
#endif
    }

    if (tearing_control_manager.has_value()) {
#if defined(WAYLAND_PROTOCOL_HAS_TEARING_CONTROL)
        tearing_control_ = wp_tearing_control_manager_v1_get_tearing_control(
                tearing_control_manager.value(), wl_surface_);
        if (tearing) {
            SPDLOG_DEBUG("[Surface] Set Presentation Hint: ASYNC");
            wp_tearing_control_v1_set_presentation_hint(tearing_control_,
                                                        WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC);
        } else {
            SPDLOG_DEBUG("[Surface] Set Presentation Hint: VSYNC");
            wp_tearing_control_v1_set_presentation_hint(tearing_control_,
                                                        WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC);
        }
#else
        (void)tearing;
#endif
    }
}

Window::~Window() {
    if (viewport_) {
#if defined(WAYLAND_PROTOCOL_HAS_VIEWPORTER)
        wp_viewport_destroy(viewport_);
#endif
    }
    if (fractional_scale_) {
#if defined(WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE)
        wp_fractional_scale_v1_destroy(fractional_scale_);
#endif
    }
    if (wl_callback_) {
        wl_callback_destroy(wl_callback_);
    }
    if (wl_surface_) {
        wl_surface_destroy(wl_surface_);
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

        new_buffer_size.width = static_cast<int>(ceil(new_buffer_size.width * fractional_buffer_scale_));
        new_buffer_size.height = static_cast<int>(ceil(new_buffer_size.height * fractional_buffer_scale_));
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

        new_buffer_size_min = std::min(new_buffer_size.width, new_buffer_size.height);
        new_buffer_size.width = new_buffer_size_min;
        new_buffer_size.height = new_buffer_size_min;

        new_viewport_dest_size_min = std::min(logical_size_.width, logical_size_.height);
        new_viewport_dest_size.width = new_viewport_dest_size_min;
        new_viewport_dest_size.height = new_viewport_dest_size_min;
    } else {
        new_viewport_dest_size.width = logical_size_.width;
        new_viewport_dest_size.height = logical_size_.height;
    }

    if (buffer_size_.width != new_buffer_size.width || buffer_size_.height != new_buffer_size.height) {
        buffer_size_.width = new_buffer_size.width;
        buffer_size_.height = new_buffer_size.height;
        if (egl_) {
            egl_->resize(buffer_size_.width, buffer_size_.height, 0, 0);
        }
    }

    if (fractional_buffer_scale_ > 0.0)
        wp_viewport_set_destination(viewport_,
                                    new_viewport_dest_size.width,
                                    new_viewport_dest_size.height);

    needs_buffer_geometry_update_ = false;
}

void Window::handle_preferred_scale(void *data,
                                    struct wp_fractional_scale_v1 *wp_fractional_scale_v1,
                                    uint32_t scale) {
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
    SPDLOG_DEBUG("handle_surface_enter: {} [{}]", fmt::ptr(wl_output), obj->name_);
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
    SPDLOG_DEBUG("handle_surface_leave: {} [{}]", fmt::ptr(output), obj->name_);
#else
    (void)output;
#endif
    obj->wl_output_ = nullptr;
}

/**
 * @brief Start rendering frames for the surface.
 *
 * This function stops the current frames (if any) and starts rendering new frames by calling the `on_frame` function.
 *
 * @note This function assumes that the surface has been initialized properly.
 */
void Window::start_frames() {
    handle_frame_callback(this, nullptr, 0);
}

/**
 * Stops the frame rendering by destroying the wl_callback object if it exists.
 * This function is intended to be called from outside the Window class.
 */
void Window::stop_frames() {
    if (wl_callback_) {
        wl_callback_destroy(wl_callback_);
    }
}

/**
 * @brief Callback function for frame completion event
 *
 * This function is called when a frame completion event is received.
 * It updates the state of the Window object and invokes the draw_frame_callback_ function.
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

    if (obj->draw_frame_callback_) {
        obj->draw_frame_callback_(data, time);
    }

    if (obj->wl_surface_) {
        obj->wl_callback_ = wl_surface_frame(obj->wl_surface_);
        wl_callback_add_listener(obj->wl_callback_, &Window::frame_callback_listener_, data);

        wl_surface_commit(obj->wl_surface_);
    }
    SPDLOG_TRACE("--Window::handle_frame_callback()");
}

void Window::handle_preferred_buffer_scale(void *data,
                                           struct wl_surface *wl_surface,
                                           int32_t factor) {
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
    auto w = static_cast<Window *>(data);
    if (w->wl_surface_ != wl_surface) {
        return;
    }
    w->preferred_buffer_transform_ = static_cast<enum wl_output_transform>(transform);
}

void Window::resize(int width, int height) {
    if (egl_) {
        logical_size_.width = width;
        logical_size_.height = height;
        egl_->resize(width, height, 0, 0);
    }
}

void Window::make_current() {
    if (egl_) {
        egl_->make_current();
    }
}

void Window::clear_current() {
    if (egl_) {
        egl_->clear_current();
    }
}

void Window::swap_buffers() {
    if (egl_) {
        egl_->swap_buffers();
    }
}
