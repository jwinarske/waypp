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

#include <algorithm>
#include <cstdint>
#include <vector>

#include <wayland-client.h>

class Touch;

class TouchObserver {
 public:
  virtual ~TouchObserver() = default;

  virtual void notify_touch_down(Touch* touch,
                                 wl_touch* wl_touch,
                                 uint32_t serial,
                                 uint32_t time,
                                 wl_surface* surface,
                                 int32_t id,
                                 wl_fixed_t x_w,
                                 wl_fixed_t y_w) = 0;

  virtual void notify_touch_up(Touch* touch,
                               wl_touch* wl_touch,
                               uint32_t serial,
                               uint32_t time,
                               int32_t id) = 0;

  virtual void notify_touch_motion(Touch* touch,
                                   wl_touch* wl_touch,
                                   uint32_t time,
                                   int32_t id,
                                   wl_fixed_t x_w,
                                   wl_fixed_t y_w) = 0;

  virtual void notify_touch_cancel(Touch* touch, wl_touch* wl_touch) = 0;

  virtual void notify_touch_frame(Touch* touch, wl_touch* wl_touch) = 0;
};

class Touch {
 public:
  struct event_mask {
    bool enabled;
    bool all;
  };

  explicit Touch(wl_touch* wl_touch, event_mask& event_mask);

  ~Touch();

  void register_observer(TouchObserver* observer) {
    observers_.push_back(observer);
  }

  void unregister_observer(TouchObserver* observer) {
    observers_.erase(
        std::remove(observers_.begin(), observers_.end(), observer),
        observers_.end());
  }

  void set_event_mask(event_mask& event_mask);

  // Disallow copy and assign.
  Touch(const Touch&) = delete;

  Touch& operator=(const Touch&) = delete;

 private:
  struct wl_touch* touch_;
  std::vector<TouchObserver*> observers_{};

  event_mask event_mask_{};

  static void handle_down(void* data,
                          wl_touch* wl_touch,
                          uint32_t serial,
                          uint32_t time,
                          wl_surface* surface,
                          int32_t id,
                          wl_fixed_t x_w,
                          wl_fixed_t y_w);

  static void handle_up(void* data,
                        wl_touch* wl_touch,
                        uint32_t serial,
                        uint32_t time,
                        int32_t id);

  static void handle_motion(void* data,
                            wl_touch* wl_touch,
                            uint32_t time,
                            int32_t id,
                            wl_fixed_t x_w,
                            wl_fixed_t y_w);

  static void handle_cancel(void* data, wl_touch* wl_touch);

  static void handle_frame(void* data, wl_touch* wl_touch);

  static constexpr wl_touch_listener listener_ = {
      .down = handle_down,
      .up = handle_up,
      .motion = handle_motion,
      .frame = handle_frame,
      .cancel = handle_cancel,
#if defined(WL_TOUCH_SHAPE_SINCE_VERSION)
      .shape = nullptr,
#endif
#if defined(WL_TOUCH_ORIENTATION_SINCE_VERSION)
      .orientation = nullptr,
#endif
  };
};
