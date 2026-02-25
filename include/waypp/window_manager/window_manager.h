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
#include <vector>

#include <EGL/egl.h>

#include "registrar.h"

class Registrar;

class WindowManager;

class WindowManagerObserver {
 public:
  virtual ~WindowManagerObserver() = default;

  virtual void notify_task() = 0;
};

class WindowManager : public Registrar {
 public:
  explicit WindowManager(
      wl_display* display,
      bool disable_cursor = false,
      unsigned long ext_interface_count = 0,
      const Registrar::RegistrarCallback* ext_interface_data = nullptr,
      GMainContext* context = nullptr);

  ~WindowManager();

  [[nodiscard]] int poll_events(int timeout) const;

  [[maybe_unused]] [[nodiscard]] int dispatch(int timeout) const;

  [[nodiscard]] int dispatch_pending() const {
    return wl_display_dispatch_pending(get_display());
  }

  [[nodiscard]] int display_dispatch() const;

  [[nodiscard]] bool has_subcompositor() const { return get_compositor(); }

  void register_task_observer(WindowManagerObserver* observer) {
    observers_.push_back(observer);
  }

  void unregister_task_observer(WindowManagerObserver* observer) {
    observers_.erase(
        std::remove(observers_.begin(), observers_.end(), observer),
        observers_.end());
  }

  [[nodiscard]] wl_output* get_primary_output() const;

  [[nodiscard]] wl_output* find_output_by_name(
      const std::string& output_name) const;

  // Disallow copy and assign.
  WindowManager(const WindowManager&) = delete;

  WindowManager& operator=(const WindowManager&) = delete;

 private:
  GMainContext* context_;

  std::vector<WindowManagerObserver*> observers_{};

  struct {
    int width;
    int height;
  } buffer_size_{};

  const std::map<wl_output*, std::unique_ptr<Output>>& outputs_;

  wl_output_transform buffer_transform_{};

  int32_t buffer_scale_ = 1;
  double fractional_buffer_scale_ = 1.0;

  struct wl_display* get_display(const char* name);
};
