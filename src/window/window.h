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

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>

#include <wayland-client.h>

#include "window_manager/xdg_window_manager.h"
#include "egl.h"

class Egl;

class Output;

class WindowManager;

class XdgTopLevel;

class Window {
public:

    enum WindowState {
        WINDOW_STATE_NONE = 0,
        WINDOW_STATE_ACTIVE = 1 << 0,
        WINDOW_STATE_MAXIMIZED = 1 << 1,
        WINDOW_STATE_FULLSCREEN = 1 << 2,
        WINDOW_STATE_TILED_LEFT = 1 << 3,
        WINDOW_STATE_TILED_RIGHT = 1 << 4,
        WINDOW_STATE_TILED_TOP = 1 << 5,
        WINDOW_STATE_TILED_BOTTOM = 1 << 6,
        WINDOW_STATE_SUSPENDED = 1 << 7,
        WINDOW_STATE_RESIZING = 1 << 8,
    };

    Window(WindowManager *wm, struct wl_compositor *wl_compositor,
           const std::optional<struct wp_viewporter *> &viewporter,
           const std::optional<struct wp_fractional_scale_manager_v1 *> &fractional_scale_manager,
           const std::optional<struct wp_tearing_control_manager_v1 *> &tearing_control_manager,
           const std::map<struct wl_output *, std::unique_ptr<Output>> &outputs,
           const char *name, const std::function<void(void *, const uint32_t)> &draw_frame_callback,
           int width, int height, wl_output_transform buffer_transform, bool fullscreen,
           bool maximized, bool fullscreen_ratio, bool tearing,
           const int32_t *context_attribs, size_t context_attribs_size,
           const int32_t *config_attribs, size_t config_attribs_size,
           int buffer_bpp, int swap_interval);

    ~Window();

    void resize(int width, int height);

    void update_buffer_geometry();

    void set_fullscreen(bool fullscreen) { fullscreen_ = fullscreen; }

    [[nodiscard]] bool get_fullscreen() const { return fullscreen_; }

    void set_maximized(bool maximized) { maximized_ = maximized; }

    [[nodiscard]] bool get_maximized() const { return maximized_; }

    void set_fullscreen_ratio(bool fullscreen_ratio) { fullscreen_ratio_ = fullscreen_ratio; }

    [[nodiscard]] bool get_fullscreen_ratio() const { return maximized_; }

    [[nodiscard]] enum wl_output_transform get_buffer_transform() const { return buffer_transform_; }

    [[nodiscard]] int32_t get_buffer_scale() const { return buffer_scale_; }

    [[nodiscard]] double get_fractional_buffer_scale() const { return fractional_buffer_scale_; }

    [[nodiscard]] struct wl_surface* get_surface() const { return wl_surface_; }

    [[nodiscard]] int get_width() const { return buffer_size_.width; }

    [[nodiscard]] int get_height() const { return buffer_size_.height; }

    void start_frames();

    void stop_frames();

    void make_current();

    void clear_current();

    void swap_buffers();

    void set_needs_buffer_geometry_update() { needs_buffer_geometry_update_ = true; }

private:
    friend XdgTopLevel;

    WindowManager *wm_;
    const std::map<struct wl_output *, std::unique_ptr<Output>> &outputs_;
    struct wp_tearing_control_v1 *tearing_control_;
    wl_output_transform buffer_transform_;
    struct wl_output *wl_output_{};

    std::string name_;
    struct wl_surface *wl_surface_;
    struct wl_callback *wl_callback_{};
    std::function<void(void *userdata, const uint32_t time)> draw_frame_callback_;

    std::unique_ptr<Egl> egl_;

    bool fullscreen_;
    bool maximized_;
    bool fullscreen_ratio_;

    int buffer_bpp_ = 0;
    int swap_interval_ = 1;
    int delay_ = 0;

    int32_t buffer_scale_ = 1;
    int32_t preferred_buffer_scale_ = 1;
    enum wl_output_transform preferred_buffer_transform_ = WL_OUTPUT_TRANSFORM_NORMAL;
    double fractional_buffer_scale_ = 1.0;

    struct {
        int width;
        int height;
    } buffer_size_;

    struct {
        int width;
        int height;
    } window_size_;



    struct {
        int width;
        int height;
    } logical_size_;

    bool needs_buffer_geometry_update_;

    static void handle_surface_enter(void *data,
                                     struct wl_surface *surface,
                                     struct wl_output *output);

    static void handle_surface_leave(void *data,
                                     struct wl_surface *surface,
                                     struct wl_output *output);

    static void handle_preferred_buffer_scale(void *data,
                                              struct wl_surface *wl_surface,
                                              int32_t factor);

    static void handle_preferred_buffer_transform(void *data,
                                                  struct wl_surface *wl_surface,
                                                  uint32_t transform);

    static constexpr struct wl_surface_listener surface_listener_ = {
            .enter = handle_surface_enter,
            .leave = handle_surface_leave
#if defined(WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION)
            ,
            .preferred_buffer_scale = handle_preferred_buffer_scale
#endif
#if defined(WL_SURFACE_PREFERRED_BUFFER_TRANSFORM_SINCE_VERSION)
            ,
            .preferred_buffer_transform = handle_preferred_buffer_transform,
#endif
    };

    struct wp_viewport *viewport_;

    struct wp_fractional_scale_v1 *fractional_scale_;

    static void handle_preferred_scale(void *data,
                                       struct wp_fractional_scale_v1 *wp_fractional_scale_v1,
                                       uint32_t scale);

#if defined(WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE)
    static constexpr struct wp_fractional_scale_v1_listener fractional_scale_listener_ = {
            .preferred_scale = handle_preferred_scale,
    };
#endif

    static void handle_frame_callback(void *data, struct wl_callback *callback, uint32_t time);

    static constexpr struct wl_callback_listener frame_callback_listener_ = {
            .done = handle_frame_callback
    };
};
