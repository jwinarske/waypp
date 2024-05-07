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

#include "agl_shell.h"

#include <algorithm>
#include <list>

#include "logging.h"

/**
 * @class AglShell
 *
 * @brief AglShell represents a Shell  for a Wayland-based display.
 *
 * The AglShell class is responsible for managing application windows using the
 * XDG Shell protocol.
 */
AglShell::AglShell(struct wl_display* display,
                   bool disable_cursor,
                   unsigned long ext_interface_count,
                   const Registrar::RegistrarCallback* ext_interface_data,
                   GMainContext* context)
    : XdgWindowManager(display,
                       disable_cursor,
                       ext_interface_count,
                       ext_interface_data,
                       context),
      wait_for_bound_(true),
      bound_ok_(false) {
  agl_shell_ = get_agl_shell();
  if (!agl_shell_) {
    spdlog::critical("{} is required.", agl_shell_interface.name);
    exit(EXIT_FAILURE);
  }

  agl_shell_add_listener(agl_shell_, &agl_shell_listener_, this);

  int ret = 0;
  while (ret != -1 && wait_for_bound_) {
    ret = wl_display_dispatch(get_display());
    if (wait_for_bound_)
      continue;
  }
  if (!bound_ok_) {
    spdlog::critical(
        "agl_shell extension already in use by other shell client.");
    exit(EXIT_FAILURE);
  }
}

AglShell::~AglShell() = default;

void AglShell::handle_bound_ok(void* data, struct agl_shell* agl_shell) {
  auto* obj = static_cast<AglShell*>(data);
  if (obj->agl_shell_ != agl_shell) {
    return;
  }

  SPDLOG_DEBUG("AglShell::handle_bound_ok");

  obj->wait_for_bound_ = false;
  obj->bound_ok_ = true;
}

void AglShell::activate_app(const std::string& app_id) {
  SPDLOG_DEBUG("[AGL] activate_app: {}", app_id);

  struct wl_output* wl_output{};

  auto it =
      std::find_if(std::begin(pending_app_list_), std::end(pending_app_list_),
                   [&](const std::pair<std::string, std::string>& p) {
                     return p.first == app_id;
                   });

  if (it != pending_app_list_.end()) {
    SPDLOG_DEBUG("[AGL] pending: {}", app_id);

    wl_output = find_output_by_name(it->second);
    if (!wl_output) {
      // try with remoting-remote-X which is the streaming
      wl_output = find_output_by_name("remoting-" + it->second);
      if (!wl_output) {
        SPDLOG_DEBUG("[AGL] Not activating app_id {} at all", app_id);
        return;
      }
    }
    pending_app_list_.erase(it);
  }

  SPDLOG_DEBUG("[AGL] Activating app_id {} on output {}", app_id, it->second);
  agl_shell_activate_app(agl_shell_, app_id.c_str(), wl_output);
  wl_display_flush(get_display());
}

void AglShell::deactivate_app(const std::string& app_id) {
  auto it = std::find_if(std::begin(apps_stack_), std::end(apps_stack_),
                         [&](const std::string& app) { return app == app_id; });

  if (it != apps_stack_.end()) {
    apps_stack_.remove(*it);
  } else {
    activate_app(apps_stack_.back());
  }
}

void AglShell::add_app_to_stack(const std::string& app_id) {
  auto it = std::find(apps_stack_.begin(), apps_stack_.end(), app_id);
  if (it == apps_stack_.end()) {
    SPDLOG_DEBUG("[AGL] adding {} to apps_stack_", app_id);
    apps_stack_.push_back(app_id);
  }
}

void AglShell::handle_bound_fail(void* data, struct agl_shell* agl_shell) {
  auto* obj = static_cast<AglShell*>(data);
  if (obj->agl_shell_ != agl_shell) {
    return;
  }

  SPDLOG_DEBUG("AglShell::handle_bound_fail");

  obj->wait_for_bound_ = false;
  obj->bound_ok_ = false;
}

void AglShell::handle_app_state(void* data,
                                struct agl_shell* agl_shell,
                                const char* app_id,
                                uint32_t state) {
  auto* obj = static_cast<AglShell*>(data);
  if (obj->agl_shell_ != agl_shell) {
    return;
  }

  switch (state) {
    case AGL_SHELL_APP_STATE_STARTED:
      SPDLOG_DEBUG("[AGL] app_id: {}, AGL_SHELL_APP_STATE_STARTED", app_id);
      obj->activate_app(app_id);
      break;
    case AGL_SHELL_APP_STATE_TERMINATED:
      SPDLOG_DEBUG("[AGL] app_id: {}, AGL_SHELL_APP_STATE_TERMINATED", app_id);
      obj->deactivate_app(app_id);
      break;
    case AGL_SHELL_APP_STATE_ACTIVATED:
      SPDLOG_DEBUG("[AGL] app_id: {}, AGL_SHELL_APP_STATE_ACTIVATED", app_id);
      obj->add_app_to_stack(app_id);
      break;
    case AGL_SHELL_APP_STATE_DEACTIVATED:
      SPDLOG_DEBUG("[AGL] app_id: {}, AGL_SHELL_APP_STATE_DEACTIVATED", app_id);
      break;
    default:
      break;
  }
}

void AglShell::handle_app_on_output(void* data,
                                    struct agl_shell* agl_shell,
                                    const char* app_id,
                                    const char* output_name) {
  auto* obj = static_cast<AglShell*>(data);
  if (obj->agl_shell_ != agl_shell) {
    return;
  }

  SPDLOG_DEBUG("[AGL] app_on_output: app_id: {}, output name: {}", app_id,
               output_name);

  // a couple of use-cases, if there is no app_id in the app_list then it
  // means this is a request to map the application, from the start to a
  // different output that the default one. We'd get an
  // AGL_SHELL_APP_STATE_STARTED which will handle activation.
  //
  // if there's an app_id then it means we might have gotten an event to
  // move the application to another output; so we'd need to process it
  // by explicitly calling processAppStatusEvent() which would ultimately
  // activate the application on other output. We'd have to pick up the
  // last activated surface and activate the default output.
  //
  // finally if the outputs are identical probably that's a user-error -
  // but the compositor won't activate it again, so we don't handle that.
  obj->pending_app_list_.emplace_back(
      std::move(std::pair(app_id, output_name)));

  auto iter = obj->apps_stack_.begin();
  while (iter != obj->apps_stack_.end()) {
    if (*iter == std::string(app_id)) {
      SPDLOG_DEBUG("[AGL] move {} to another output {}", app_id, output_name);
      obj->activate_app(app_id);
      break;
    }
    iter++;
  }
}

void AglShell::set_background(struct wl_surface* wl_surface,
                              struct wl_output* wl_output) const {
  SPDLOG_DEBUG("[AGL] Set Background: surface: {}, output: {}",
               fmt::ptr(wl_surface), fmt::ptr(wl_output));
  agl_shell_set_background(agl_shell_, wl_surface, wl_output);
}

std::string AglShell::edge_to_string(const enum agl_shell_edge mode) {
  switch (mode) {
    case AGL_SHELL_EDGE_TOP:
      return "AGL_SHELL_EDGE_TOP";
    case AGL_SHELL_EDGE_BOTTOM:
      return "AGL_SHELL_EDGE_BOTTOM";
    case AGL_SHELL_EDGE_LEFT:
      return "AGL_SHELL_EDGE_LEFT";
    case AGL_SHELL_EDGE_RIGHT:
      return "AGL_SHELL_EDGE_RIGHT";
  }
  return {};
}

void AglShell::set_panel(struct wl_surface* wl_surface,
                         struct wl_output* wl_output,
                         const enum agl_shell_edge mode) const {
  SPDLOG_DEBUG("[AGL] Set Panel: surface: {}, output: {}, mode: {}",
               fmt::ptr(wl_surface), fmt::ptr(wl_output),
               edge_to_string(mode).c_str());
  agl_shell_set_panel(agl_shell_, wl_surface, wl_output, mode);
}

void AglShell::set_activate_region(struct wl_output* wl_output,
                                   uint32_t x,
                                   uint32_t y,
                                   uint32_t width,
                                   uint32_t height) const {
  SPDLOG_DEBUG(
      "[AGL] Set Activate Region: output: {}, x: {}, y: {}, width: {}, height: "
      "{}",
      fmt::ptr(wl_output), x, y, width, height);
  agl_shell_set_activate_region(
      agl_shell_, wl_output, static_cast<int32_t>(x), static_cast<int32_t>(y),
      static_cast<int32_t>(width), static_cast<int32_t>(height));
}

void AglShell::ready() const {
  SPDLOG_DEBUG("[AGL] Ready");
  agl_shell_ready(agl_shell_);
}
