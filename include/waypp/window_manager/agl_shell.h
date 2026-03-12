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

#include "agl-shell-client-protocol.hpp"
#include "registrar.h"
#include "xdg_window_manager.h"

class XdgWindowManager;

class AglShell : public XdgWindowManager,
                 public agl_shell::client::CAglShell<AglShell> {
 public:
  explicit AglShell(
      struct wl_display* display,
      bool disable_cursor = false,
      unsigned long ext_interface_count = 0,
      const Registrar::RegistrarCallback* ext_interface_data = nullptr,
      GMainContext* context = nullptr);

  ~AglShell() = default;

  int dispatch_pending() const {
    return wl_display_dispatch_pending(get_display());
  }

  void activate_app(const std::string& app_id);

  void deactivate_app(const std::string& app_id);

  void set_background(struct wl_surface* wl_surface,
                      struct wl_output* wl_output) const;

  void set_panel(struct wl_surface* wl_surface,
                 struct wl_output* wl_output,
                 agl_shell::client::AglShellEdge mode) const;

  void set_activate_region(struct wl_output* wl_output,
                           uint32_t x,
                           uint32_t y,
                           uint32_t width,
                           uint32_t height) const;

  void ready() const;

  void process_app_status_event(const char* app_id,
                                const std::string& event_type);

  static std::string edge_to_string(agl_shell::client::AglShellEdge mode);

  // Disallow copy and assign.
  AglShell(const AglShell&) = delete;

  AglShell& operator=(const AglShell&) = delete;

 private:
  volatile bool wait_for_bound_;
  bool bound_ok_;

  std::list<std::string> apps_stack_;
  std::list<std::pair<std::string, std::string>> pending_app_list_;

  void add_app_to_stack(const std::string& app_id);

  void OnBoundOk() override;
  void OnBoundFail() override;
  void OnAppState(const char* app_id, uint32_t state) override;
  void OnAppOnOutput(const char* app_id, const char* output_name) override;
};
