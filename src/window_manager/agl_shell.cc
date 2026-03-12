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

#include "waypp/window_manager/agl_shell.h"

#include <algorithm>
#include <list>
#include <stdexcept>

#include "logging/logging.h"
#include "waypp/window_manager/registrar.h"

/**
 * @class AglShell
 *
 * @brief AglShell represents a Shell  for a Wayland-based display.
 *
 * The AglShell class is responsible for managing application windows using the
 * XDG Shell protocol.
 */
AglShell::AglShell(struct wl_display* display,
                   const bool disable_cursor,
                   const unsigned long ext_interface_count,
                   const RegistrarCallback* ext_interface_data,
                   GMainContext* context)
    : XdgWindowManager(display,
                       disable_cursor,
                       ext_interface_count,
                       ext_interface_data,
                       context),
      wait_for_bound_(true),
      bound_ok_(false) {
  _SetProxy(get_agl_shell());
  if (!GetProxy()) {
    throw std::runtime_error(
        std::string(agl_shell::client::agl_shell_traits::interface_name) +
        " protocol is required but not advertised by the compositor");
  }

  int ret = 0;
  while (ret != -1 && wait_for_bound_) {
    ret = wl_display_dispatch(get_display());
    if (wait_for_bound_)
      continue;
  }
  if (!bound_ok_) {
    throw std::runtime_error(
        "agl_shell binding failed: extension already in use by another shell "
        "client");
  }
}

void AglShell::OnBoundOk() {
  DLOG_DEBUG("AglShell::OnBoundOk");
  wait_for_bound_ = false;
  bound_ok_ = true;
}

void AglShell::activate_app(const std::string& app_id) {
  DLOG_DEBUG("[AGL] activate_app: {}", app_id);

  wl_output* wl_output{};

  const auto it =
      std::find_if(std::begin(pending_app_list_), std::end(pending_app_list_),
                   [&](const std::pair<std::string, std::string>& p) {
                     return p.first == app_id;
                   });

  if (it != pending_app_list_.end()) {
    DLOG_DEBUG("[AGL] pending: {}", app_id);

    // Save the output name before erasing — erase() invalidates the iterator.
    const std::string output_name = it->second;
    pending_app_list_.erase(it);

    wl_output = find_output_by_name(output_name);
    if (!wl_output) {
      // try with remoting-remote-X which is the streaming
      wl_output = find_output_by_name("remoting-" + output_name);
      if (!wl_output) {
        DLOG_DEBUG("[AGL] Not activating app_id {} at all", app_id);
        return;
      }
    }
    DLOG_DEBUG("[AGL] Activating app_id {} on output {}", app_id, output_name);
  }

  ActivateApp(app_id.c_str(), (wl_proxy*)wl_output);
  wl_display_flush(get_display());
}

void AglShell::deactivate_app(const std::string& app_id) {
  const auto it =
      std::find_if(std::begin(apps_stack_), std::end(apps_stack_),
                   [&](const std::string& app) { return app == app_id; });

  if (it != apps_stack_.end()) {
    apps_stack_.remove(*it);
  } else {
    activate_app(apps_stack_.back());
  }
}

void AglShell::add_app_to_stack(const std::string& app_id) {
  if (auto it = std::find(apps_stack_.begin(), apps_stack_.end(), app_id);
      it == apps_stack_.end()) {
    DLOG_DEBUG("[AGL] adding {} to apps_stack_", app_id);
    apps_stack_.push_back(app_id);
  }
}

void AglShell::OnBoundFail() {
  LOG_DEBUG("AglShell::OnBoundFail");
  wait_for_bound_ = false;
  bound_ok_ = false;
}

void AglShell::OnAppState(const char* app_id, uint32_t state) {
  using namespace agl_shell::client;
  switch (static_cast<AglShellAppState>(state)) {
    case AglShellAppState::Started:
      LOG_DEBUG("[AGL] app_id: {}, AglShellAppState::Started", app_id);
      activate_app(app_id);
      break;
    case AglShellAppState::Terminated:
      LOG_DEBUG("[AGL] app_id: {}, AglShellAppState::Terminated", app_id);
      deactivate_app(app_id);
      break;
    case AglShellAppState::Activated:
      LOG_DEBUG("[AGL] app_id: {}, AglShellAppState::Activated", app_id);
      add_app_to_stack(app_id);
      break;
    case AglShellAppState::Deactivated:
      LOG_DEBUG("[AGL] app_id: {}, AglShellAppState::Deactivated", app_id);
      break;
    default:
      break;
  }
}

void AglShell::OnAppOnOutput(const char* app_id, const char* output_name) {
  LOG_DEBUG("[AGL] app_on_output: app_id: {}, output name: {}", app_id,
            output_name);

  pending_app_list_.emplace_back(std::pair(app_id, output_name));

  auto iter = apps_stack_.begin();
  while (iter != apps_stack_.end()) {
    if (*iter == std::string(app_id)) {
      LOG_DEBUG("[AGL] move {} to another output {}", app_id, output_name);
      activate_app(app_id);
      break;
    }
    ++iter;
  }
}

void AglShell::set_background(struct wl_surface* wl_surface,
                              struct wl_output* wl_output) const {
  LOG_DEBUG("[AGL] Set Background: surface: {}, output: {}",
            fmt::ptr(wl_surface), fmt::ptr(wl_output));
  SetBackground((wl_proxy*)wl_surface, (wl_proxy*)wl_output);
}

std::string AglShell::edge_to_string(const agl_shell::client::AglShellEdge mode) {
  using namespace agl_shell::client;
  switch (mode) {
    case AglShellEdge::Top:
      return "AglShellEdge::Top";
    case AglShellEdge::Bottom:
      return "AglShellEdge::Bottom";
    case AglShellEdge::Left:
      return "AglShellEdge::Left";
    case AglShellEdge::Right:
      return "AglShellEdge::Right";
  }
  return {};
}

void AglShell::set_panel(struct wl_surface* wl_surface,
                         struct wl_output* wl_output,
                         const agl_shell::client::AglShellEdge mode) const {
  LOG_DEBUG("[AGL] Set Panel: surface: {}, output: {}, mode: {}",
            fmt::ptr(wl_surface), fmt::ptr(wl_output),
            edge_to_string(mode).c_str());
  SetPanel((wl_proxy*)wl_surface, (wl_proxy*)wl_output,
           static_cast<uint32_t>(mode));
}

void AglShell::set_activate_region(struct wl_output* wl_output,
                                   uint32_t x,
                                   uint32_t y,
                                   uint32_t width,
                                   uint32_t height) const {
  LOG_DEBUG(
      "[AGL] Set Activate Region: output: {}, x: {}, y: {}, width: {}, height: "
      "{}",
      fmt::ptr(wl_output), x, y, width, height);
  SetActivateRegion((wl_proxy*)wl_output, static_cast<int32_t>(x),
                    static_cast<int32_t>(y), static_cast<int32_t>(width),
                    static_cast<int32_t>(height));
}

void AglShell::ready() const {
  LOG_DEBUG("[AGL] Ready");
  Ready();
}

void AglShell::process_app_status_event(const char* app_id,
                                        const std::string& event_type) {
  if (!GetProxy())
    return;

  if (event_type == "started") {
    activate_app(std::string(app_id));
  } else if (event_type == "terminated") {
    deactivate_app(std::string(app_id));
  }
  // "deactivated" is not handled
}
