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
#include <vector>

#include <wayland-client.h>

#include "feedback.h"
#include "window_manager/xdg_window_manager.h"
#include "egl.h"
#include "buffer.h"

class Buffer;

class Egl;

class FeedbackObserver;

class Output;

class WindowManager;

class Window {
public:

    enum RuntimeMode {
        WINDOW_RUNTIME_MODE_FEEDBACK = 0,
        WINDOW_RUNTIME_MODE_PRESENTATION = 1 << 0,
    };

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

    Window(WindowManager *wm, const char *name, int buffer_count, uint32_t buffer_format,
           const std::function<void(void *, const uint32_t)> &draw_frame_callback, int width, int height,
           bool fullscreen, bool maximized, bool fullscreen_ratio, bool tearing, int buffer_bpp = 0,
           int swap_interval = 0, const int32_t *context_attribs = nullptr, size_t context_attribs_size = 0,
           const int32_t *config_attribs = nullptr, size_t config_attribs_size = 0,
           enum Egl::api type = Egl::OPENGL_ES_API);

    ~Window();

    [[nodiscard]] bool is_valid() const { return valid_; }

    void resize(int width, int height);

    void update_buffer_geometry();

    [[nodiscard]] enum wl_output_transform get_buffer_transform() const { return buffer_transform_; }

    [[nodiscard]] struct wl_surface *get_surface() const { return wl_surface_; }

    [[nodiscard]] int get_width() const { return logical_size_.width; }

    [[nodiscard]] int get_height() const { return logical_size_.height; }

    void set_max_width(int width) { max_width_ = width; }

    void set_max_height(int height) { max_height_ = height; }

    void set_window_width(int width) { window_size_.width = width; }

    void set_window_height(int height) { window_size_.height = height; }

    void set_init_width(int width) { init_width_ = width; }

    void set_init_height(int height) { init_height_ = height; }

    void set_width(int width) { width_ = width; }

    void set_height(int height) { height_ = height; }

    [[nodiscard]] int get_init_width() const { return init_width_; }

    [[nodiscard]] int get_init_height() const { return init_height_; }

    void set_fullscreen(bool fullscreen) { fullscreen_ = fullscreen; }

    void set_maximized(bool maximized) { maximized_ = maximized; }

    void set_resize(bool resize) { resize_ = resize; }

    void set_activated(bool activated) { activated_ = activated; }

    void set_valid(bool valid) { valid_ = valid; }

    void set_needs_buffer_geometry_update(bool value) { needs_buffer_geometry_update_ = value; }

    [[nodiscard]] int32_t get_max_width() const { return max_width_; }

    [[nodiscard]] int32_t get_max_height() const { return max_height_; }

    [[nodiscard]] bool get_fullscreen() const { return fullscreen_; }

    [[nodiscard]] bool get_maximized() const { return maximized_; }

    [[nodiscard]] void *get_user_data() const { return user_data_; }

    void set_user_data(void *user_data) { user_data_ = user_data; }

    void set_runtime_mode(RuntimeMode runtime_mode) { runtime_mode_ = runtime_mode; }

    void start_frame_callbacks();

    void stop_frame_callbacks();

    void make_current();

    void clear_current();

    void swap_buffers();

    bool have_swap_buffers_width_damage();

    void get_buffer_age(EGLint &age);

    void swap_buffers_with_damage(const EGLint *rects, EGLint n_rects);

    Buffer *pick_free_buffer();

    [[nodiscard]] size_t get_num_buffers() const { return buffers_.size(); }

    [[nodiscard]] const std::vector<std::unique_ptr<Buffer>> &get_buffers() const { return buffers_; }

    Buffer *next_buffer();

    void opaque_region_add(int32_t x, int32_t y, int32_t width, int32_t height);

    void opaque_region_clear();

    void presentation_feedback_add_callbacks();

    // Disallow copy and assign.
    Window(const Window &) = delete;

    Window &operator=(const Window &) = delete;

private:
    WindowManager *wm_;
    const std::map<struct wl_output *, std::unique_ptr<Output>> &outputs_;
    struct wp_tearing_control_v1 *tearing_control_{};
    wl_output_transform buffer_transform_;
    struct wl_output *wl_output_{};
    RuntimeMode runtime_mode_;

    struct {
        struct wp_presentation *wp_presentation;
        clockid_t clock_id;
        std::list<std::unique_ptr<Feedback>> feedback_list;
    } presentation_{};

    std::string name_;
    struct wl_surface *wl_surface_;
    struct wl_callback *wl_callback_{};
    std::function<void(void *userdata, const uint32_t time)> frame_callback_;
    void *user_data_{};

    std::unique_ptr<Egl> egl_;

    std::vector<std::unique_ptr<Buffer>> buffers_;

    bool fullscreen_;
    bool maximized_;
    bool fullscreen_ratio_;
    bool valid_{};

    bool resize_{};
    bool activated_{};

    int buffer_bpp_ = 0;
    int swap_interval_ = 1;
    int delay_ = 0;

    int32_t buffer_scale_ = 1;
    int32_t preferred_buffer_scale_ = 1;
    enum wl_output_transform preferred_buffer_transform_ = WL_OUTPUT_TRANSFORM_NORMAL;
    double fractional_buffer_scale_ = 1.0;

    int buffer_count_;
    uint32_t buffer_format_;

    int init_width_{};
    int init_height_{};

    int width_{};
    int height_{};

    int max_width_ = INT32_MAX;
    int max_height_ = INT32_MAX;

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
#if WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION
            ,
            .preferred_buffer_scale = handle_preferred_buffer_scale
#endif
#if WL_SURFACE_PREFERRED_BUFFER_TRANSFORM_SINCE_VERSION
            ,
            .preferred_buffer_transform = handle_preferred_buffer_transform,
#endif
    };

    struct wp_viewport *viewport_{};

    struct wp_fractional_scale_v1 *fractional_scale_{};

    static void handle_preferred_scale(void *data,
                                       struct wp_fractional_scale_v1 *wp_fractional_scale_v1,
                                       uint32_t scale);

#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1
    static constexpr struct wp_fractional_scale_v1_listener fractional_scale_listener_ = {
            .preferred_scale = handle_preferred_scale,
    };
#endif

    static void handle_frame_callback(void *data, struct wl_callback *callback, uint32_t time);

    static constexpr struct wl_callback_listener frame_callback_listener_ = {
            .done = handle_frame_callback
    };
};
