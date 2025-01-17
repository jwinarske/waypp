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

#include <list>
#include <string>
#include <vector>

#include <wayland-client.h>

class Pointer;

class PointerObserver {
 public:
  virtual ~PointerObserver() = default;

  virtual void notify_pointer_enter(Pointer* pointer,
                                    wl_pointer* wl_pointer,
                                    uint32_t serial,
                                    wl_surface* surface,
                                    double sx,
                                    double sy) = 0;

  virtual void notify_pointer_leave(Pointer* pointer,
                                    wl_pointer* wl_pointer,
                                    uint32_t serial,
                                    wl_surface* surface) = 0;

  virtual void notify_pointer_motion(Pointer* pointer,
                                     wl_pointer* wl_pointer,
                                     uint32_t time,
                                     double sx,
                                     double sy) = 0;

  virtual void notify_pointer_button(Pointer* pointer,
                                     wl_pointer* wl_pointer,
                                     uint32_t serial,
                                     uint32_t time,
                                     uint32_t button,
                                     uint32_t state) = 0;

  virtual void notify_pointer_axis(Pointer* pointer,
                                   wl_pointer* wl_pointer,
                                   uint32_t time,
                                   uint32_t axis,
                                   double value) = 0;

  virtual void notify_pointer_frame(Pointer* pointer,
                                    wl_pointer* wl_pointer) = 0;

  virtual void notify_pointer_axis_source(Pointer* pointer,
                                          wl_pointer* wl_pointer,
                                          uint32_t axis_source) = 0;

  virtual void notify_pointer_axis_stop(Pointer* pointer,
                                        wl_pointer* wl_pointer,
                                        uint32_t time,
                                        uint32_t axis) = 0;

  virtual void notify_pointer_axis_discrete(Pointer* pointer,
                                            wl_pointer* wl_pointer,
                                            uint32_t axis,
                                            int32_t discrete) = 0;
};

class Pointer {
 public:
  static constexpr int kResizeMargin = 10;

  struct event_mask {
    bool enabled;
    bool all;
    bool axis;
    bool buttons;
    bool motion;
  };

  explicit Pointer(wl_pointer* pointer,
                   wl_compositor* wl_compositor,
                   wl_shm* wl_shm,
                   bool disable_cursor,
                   const event_mask& event_mask,
                   int size = 24);

  ~Pointer();

  void register_observer(PointerObserver* observer, void* user_data = nullptr) {
    observers_.push_back(observer);

    if (user_data) {
      user_data_ = user_data;
    }
  }

  void unregister_observer(PointerObserver* observer) {
    observers_.remove(observer);
  }

  void set_user_data(void* user_data) { user_data_ = user_data; }

  [[nodiscard]] void* get_user_data() const { return user_data_; }

  static std::string get_cursor_theme();

  static std::vector<std::string> get_available_cursors(
      const char* theme_name = nullptr);

  void set_cursor(uint32_t serial,
                  const char* cursor_name = "right_ptr",
                  const char* theme_name = nullptr);

  [[nodiscard]] bool is_cursor_enabled() const { return !disable_cursor_; }

  void set_event_mask(const event_mask& event_mask);

  [[nodiscard]] std::pair<double, double> get_xy() const { return {sx_, sy_}; }

  // Disallow copy and assign.
  Pointer(const Pointer&) = delete;

  Pointer& operator=(const Pointer&) = delete;

 private:
  struct wl_pointer* wl_pointer_;
  std::list<PointerObserver*> observers_{};
  struct wl_surface* wl_surface_cursor_;
  struct wl_cursor_theme* theme_{};
  wl_shm* wl_shm_;
  bool disable_cursor_;
  int size_;
  void* user_data_{};

  double sx_{};
  double sy_{};

#if ENABLE_XDG_CLIENT
  enum xdg_toplevel_resize_edge prev_resize_edge_ =
      XDG_TOPLEVEL_RESIZE_EDGE_NONE;
#endif

  event_mask event_mask_{};

  static void handle_enter(void* data,
                           wl_pointer* pointer,
                           uint32_t serial,
                           wl_surface* surface,
                           wl_fixed_t sx,
                           wl_fixed_t sy);

  static void handle_leave(void* data,
                           wl_pointer* pointer,
                           uint32_t serial,
                           wl_surface* surface);

  static void handle_motion(void* data,
                            wl_pointer* pointer,
                            uint32_t time,
                            wl_fixed_t sx,
                            wl_fixed_t sy);

  static void handle_button(void* data,
                            wl_pointer* wl_pointer,
                            uint32_t serial,
                            uint32_t time,
                            uint32_t button,
                            uint32_t state);

  static void handle_axis(void* data,
                          wl_pointer* wl_pointer,
                          uint32_t time,
                          uint32_t axis,
                          wl_fixed_t value);

  static void handle_frame(void* data, wl_pointer* wl_pointer);

  static void handle_axis_source(void* data,
                                 wl_pointer* wl_pointer,
                                 uint32_t axis_source);

  static void handle_axis_stop(void* data,
                               wl_pointer* wl_pointer,
                               uint32_t time,
                               uint32_t axis);

  static void handle_axis_discrete(void* data,
                                   wl_pointer* wl_pointer,
                                   uint32_t axis,
                                   int32_t discrete);

  static constexpr wl_pointer_listener pointer_listener_ = {
      .enter = handle_enter,
      .leave = handle_leave,
      .motion = handle_motion,
      .button = handle_button,
      .axis = handle_axis,
      .frame = handle_frame,
      .axis_source = handle_axis_source,
      .axis_stop = handle_axis_stop,
      .axis_discrete = handle_axis_discrete,
#if defined(WL_POINTER_AXIS_VALUE120_SINCE_VERSION)
      .axis_value120 = nullptr,
#endif
#if defined(WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION)
      .axis_relative_direction = nullptr,
#endif
  };
};
