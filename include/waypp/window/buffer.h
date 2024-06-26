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

#include <wayland-client-core.h>
#include <cstdint>

class Buffer {
 public:
  explicit Buffer(struct wl_shm* wl_shm);

  ~Buffer();

  [[nodiscard]] int get_width() const { return width_; }

  [[nodiscard]] int get_height() const { return height_; }

  [[nodiscard]] uint32_t get_format() const { return format_; }

  int create_shm_buffer(int width, int height, uint32_t format);

  [[nodiscard]] bool is_busy() const { return busy_; }

  [[nodiscard]] void* get_shm_data() const { return shm_data_; }

  [[nodiscard]] struct wl_buffer* get_wl_buffer() const { return buffer_; }

  [[nodiscard]] uint32_t get_id() const {
    return wl_proxy_get_id(reinterpret_cast<wl_proxy*>(buffer_));
  }

  void set_busy() { busy_ = true; }

  // Disallow copy and assign.
  Buffer(const Buffer&) = delete;

  Buffer& operator=(const Buffer&) = delete;

 private:
  int width_;
  int height_;
  uint32_t format_{};
  bool busy_;

  int size_{};
  wl_shm* wl_shm_;
  wl_buffer* buffer_{};
  void* shm_data_{};

  static void handle_release(void* data, wl_buffer* buffer);

  static const struct wl_buffer_listener listener_;
};