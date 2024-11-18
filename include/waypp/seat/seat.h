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

#include <memory>
#include <optional>
#include <string>

#include <wayland-client.h>

#include "keyboard.h"
#include "pointer.h"
#include "touch.h"

class Keyboard;

class Pointer;

class Touch;

class Seat;

class SeatObserver {
 public:
  virtual ~SeatObserver() = default;

  virtual void notify_seat_name(Seat* seat,
                                wl_seat* wl_seat,
                                const char* name) = 0;

  virtual void notify_seat_capabilities(Seat* seat,
                                        wl_seat* wl_seat,
                                        uint32_t caps) = 0;
};

class Seat {
 public:
  struct event_mask {
    Pointer::event_mask pointer;
    Keyboard::event_mask keyboard;
    Touch::event_mask touch;
  };

  explicit Seat(wl_seat* seat,
                wl_shm* wl_shm,
                wl_compositor* wl_compositor,
                bool disable_cursor,
                const char* ignore_events = nullptr);

  ~Seat();

  void register_observer(SeatObserver* observer, void* user_data = nullptr) {
    observers_.push_back(observer);

    if (user_data) {
      user_data_ = user_data;
    }
  }

  void unregister_observer(SeatObserver* observer) {
    observers_.remove(observer);
  }

  [[nodiscard]] void* get_user_data() const { return user_data_; }

  [[nodiscard]] wl_seat* get_seat() const { return wl_seat_; }

  [[nodiscard]] uint32_t get_capabilities() const { return capabilities_; }

  [[nodiscard]] const std::string& get_name() const { return name_; }

  [[nodiscard]] std::optional<Keyboard*> get_keyboard() const;

  [[nodiscard]] std::optional<Pointer*> get_pointer() const;

  void set_event_mask(const char* ignore_events);

  // Disallow copy and assign.
  Seat(const Seat&) = delete;

  Seat& operator=(const Seat&) = delete;

 private:
  struct wl_seat* wl_seat_;
  uint32_t capabilities_{};
  std::string name_;
  struct wl_shm* wl_shm_;
  struct wl_compositor* wl_compositor_;
  bool disable_cursor_;
  void* user_data_{};
  event_mask event_mask_{};

  std::list<SeatObserver*> observers_{};

  std::unique_ptr<Keyboard> keyboard_;
  std::unique_ptr<Pointer> pointer_;
  std::unique_ptr<Touch> touch_;

  static void handle_capabilities(void* data, wl_seat* wl_seat, uint32_t caps);

  static void handle_name(void* data, wl_seat* wl_seat, const char* name);

  static constexpr wl_seat_listener listener_ = {
      .capabilities = handle_capabilities,
      .name = handle_name,
  };

  void event_mask_print() const;
};
