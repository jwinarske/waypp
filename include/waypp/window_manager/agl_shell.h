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

#include "agl-shell-client-protocol.h"
#include "registrar.h"
#include "xdg_window_manager.h"

class XdgWindowManager;

class AglShell : public XdgWindowManager {
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
                 enum agl_shell_edge mode) const;

  void set_activate_region(struct wl_output* wl_output,
                           uint32_t x,
                           uint32_t y,
                           uint32_t width,
                           uint32_t height) const;

  void ready() const;

  void process_app_status_event(const char* app_id,
                                const std::string& event_type);

  static std::string edge_to_string(const enum agl_shell_edge mode);

  // Disallow copy and assign.
  AglShell(const AglShell&) = delete;

  AglShell& operator=(const AglShell&) = delete;

 private:
  struct agl_shell* agl_shell_;
  volatile bool wait_for_bound_;
  bool bound_ok_;

  std::list<std::string> apps_stack_;
  std::list<std::pair<std::string, std::string>> pending_app_list_;

  /**
   * event sent if binding was ok
   *
   * Informs the client that it was able to bind the agl_shell
   * interface succesfully. Clients are required to wait for this
   * event before continuing further.
   * @since 2
   */
  static void handle_bound_ok(void* data, struct agl_shell* agl_shell);

  /**
   * event sent if binding was nok
   *
   * Informs the client that binding to the agl_shell interface was
   * unsuccesfull. Clients are required to wait for this event for
   * continuing further.
   * @since 2
   */
  static void handle_bound_fail(void* data, struct agl_shell* agl_shell);

  /**
   * event sent when an application suffered state modification
   *
   * Informs the client that an application has changed its state
   * to another, specified by the app_state enum. Client can use this
   * event to track current application state. For instance to know
   * when the application has started, or when terminated/stopped.
   * @since 3
   */
  static void handle_app_state(void* data,
                               struct agl_shell* agl_shell,
                               const char* app_id,
                               uint32_t state);

  /**
   * Event sent as a reponse to set_app_output
   *
   * Clients can use this event to be notified when an application
   * wants to be displayed on a certain output. This event is sent in
   * response to the set_app_output request.
   *
   * See xdg_toplevel.set_app_id from the xdg-shell protocol for a
   * description of app_id.
   * @since 8
   */
  static void handle_app_on_output(void* data,
                                   struct agl_shell* agl_shell,
                                   const char* app_id,
                                   const char* output_name);

  void add_app_to_stack(const std::string& app_id);

  static constexpr struct agl_shell_listener agl_shell_listener_ = {
      .bound_ok = handle_bound_ok,
      .bound_fail = handle_bound_fail,
      .app_state = handle_app_state,
      .app_on_output = handle_app_on_output,
  };
};
