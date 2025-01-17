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
#include <vector>

#include <wayland-client.h>

#include "egl.h"
#include "feedback.h"
#include "waypp/window/buffer.h"
#include "waypp/window_manager/xdg_window_manager.h"

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

  Window(std::shared_ptr<WindowManager> wm,
         const char* name,
         int buffer_count,
         uint32_t buffer_format,
         const std::function<void(void*, uint32_t)>& frame_callback,
         int width,
         int height,
         bool fullscreen,
         bool maximized,
         bool fullscreen_ratio,
         bool tearing,
         Egl::config* egl_config);

  ~Window();

  [[nodiscard]] bool is_valid() const { return valid_; }

  void close() { valid_ = false; }

  void resize(int width, int height);

  void update_buffer_geometry();

  [[nodiscard]] wl_output_transform get_buffer_transform() const {
    return buffer_transform_;
  }

  [[nodiscard]] wl_surface* get_surface() const { return wl_surface_; }

  [[nodiscard]] int get_width() const { return extents_.logical.width; }

  [[nodiscard]] int get_height() const { return extents_.logical.height; }

  void set_max_width(const int width) { extents_.max.width = width; }

  void set_max_height(const int height) { extents_.max.height = height; }

  void set_width(const int width) { extents_.logical.width = width; }

  void set_height(const int height) { extents_.logical.height = height; }

  void set_init_width(const int width) { extents_.init.width = width; }

  void set_init_height(const int height) { extents_.init.height = height; }

  [[nodiscard]] int get_init_width() const { return extents_.init.width; }

  [[nodiscard]] int get_init_height() const { return extents_.init.height; }

  void set_fullscreen(const bool fullscreen) { fullscreen_ = fullscreen; }

  void set_maximized(const bool maximized) { maximized_ = maximized; }

  void set_resizing(const bool resizing) { resizing_ = resizing; }

  [[nodiscard]] bool get_resizing() const { return resizing_; }

  void set_activated(const bool activated) { activated_ = activated; }

  void set_valid(const bool valid) { valid_ = valid; }

  void set_needs_buffer_geometry_update(const bool value) {
    needs_buffer_geometry_update_ = value;
  }

  [[nodiscard]] int32_t get_max_width() const { return extents_.max.width; }

  [[nodiscard]] int32_t get_max_height() const { return extents_.max.height; }

  [[nodiscard]] bool get_fullscreen() const { return fullscreen_; }

  [[nodiscard]] bool get_maximized() const { return maximized_; }

  [[nodiscard]] void* get_user_data() const { return user_data_; }

  void set_user_data(void* user_data) { user_data_ = user_data; }

  void set_runtime_mode(const RuntimeMode runtime_mode) {
    runtime_mode_ = runtime_mode;
  }

  void start_frame_callbacks(void* user_data = nullptr);

  void stop_frame_callbacks();

  void make_current() const;

  void clear_current() const;

  void swap_buffers() const;

  [[nodiscard]] bool have_swap_buffers_width_damage() const;

  void get_buffer_age(EGLint& buffer_age) const;

  void swap_buffers_with_damage(EGLint* rects, EGLint n_rects) const;

  [[nodiscard]] Buffer* pick_free_buffer() const;

  [[nodiscard]] size_t get_num_buffers() const { return buffers_.size(); }

  [[nodiscard]] const std::vector<std::unique_ptr<Buffer>>& get_buffers()
      const {
    return buffers_;
  }

  [[nodiscard]] Buffer* next_buffer() const;

  void opaque_region_add(int32_t x, int32_t y, int32_t width, int32_t height);

  void opaque_region_clear() const;

  static void presentation_feedback_add_callbacks();

  // Disallow copy and assign.
  Window(const Window&) = delete;

  Window& operator=(const Window&) = delete;

 private:
  struct Extents {
    int width;
    int height;
  };

  std::shared_ptr<WindowManager> wm_;
  const std::map<wl_output*, std::unique_ptr<Output>>& outputs_;
#if HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1
  wp_tearing_control_v1* tearing_control_{};
#endif
  wl_output_transform buffer_transform_;
  wl_output* wl_output_{};
  RuntimeMode runtime_mode_;

  struct {
    struct wp_presentation* wp_presentation;
    clockid_t clock_id;
    std::list<std::unique_ptr<Feedback>> feedback_list;
  } presentation_{};

  std::string name_;
  wl_surface* wl_surface_;
  wl_callback* wl_callback_{};
  std::function<void(void* userdata, uint32_t time)> frame_callback_;
  void* user_data_{};

#if ENABLE_EGL
  std::unique_ptr<Egl> egl_;
#endif

  std::vector<std::unique_ptr<Buffer>> buffers_;

  bool fullscreen_;
  bool maximized_;
  bool fullscreen_ratio_;
  bool valid_{};

  bool resizing_{};
  bool activated_{};

  int32_t buffer_scale_ = 1;
  int32_t preferred_buffer_scale_ = 1;
  wl_output_transform preferred_buffer_transform_ = WL_OUTPUT_TRANSFORM_NORMAL;
  double fractional_buffer_scale_ = 1.0;

  int buffer_count_;
  uint32_t buffer_format_;

  struct {
    Extents init;
    Extents max;
    Extents buffer;
    Extents window;
    Extents logical;
  } extents_{};

  bool needs_buffer_geometry_update_;

  static void handle_surface_enter(void* data,
                                   wl_surface* surface,
                                   wl_output* output);

  static void handle_surface_leave(void* data,
                                   wl_surface* surface,
                                   wl_output* output);

  static void handle_preferred_buffer_scale(void* data,
                                            wl_surface* wl_surface,
                                            int32_t factor);

  static void handle_preferred_buffer_transform(void* data,
                                                wl_surface* wl_surface,
                                                uint32_t transform);

  static constexpr wl_surface_listener surface_listener_ = {
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

  wp_viewport* viewport_{};

  struct wp_fractional_scale_v1* fractional_scale_{};

  static void handle_preferred_scale(
      void* data,
      wp_fractional_scale_v1* wp_fractional_scale_v1,
      uint32_t scale);

#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1
  static constexpr struct wp_fractional_scale_v1_listener
      fractional_scale_listener_ = {
          .preferred_scale = handle_preferred_scale,
  };
#endif

  static void handle_frame_callback(void* data,
                                    wl_callback* callback,
                                    uint32_t time);

  static constexpr wl_callback_listener frame_callback_listener_ = {
      .done = handle_frame_callback};
};
