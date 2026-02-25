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

#include "waypp/window_manager/window_manager.h"

#include <poll.h>
#include <wayland-client.h>

#include "logging/logging.h"
#include "waypp/window_manager/registrar.h"

class Registrar;

/**
 * @class WindowManager
 *
 * @brief The WindowManager class is responsible for managing windows and
 * handling surface-related operations.
 *
 * The WindowManager class extends the Display and Window classes and is used to
 * create and manage windows in a graphical user interface application.
 *
 * @see Display
 * @see Window
 * @see XdgWm
 */
WindowManager::WindowManager(wl_display* display,
                             const bool disable_cursor,
                             const unsigned long ext_interface_count,
                             const RegistrarCallback* ext_interface_data,
                             GMainContext* context)
    : Registrar(display,
                ext_interface_count,
                ext_interface_data,
                disable_cursor),
      context_(context),
      outputs_(get_outputs()) {
  DLOG_TRACE("++WindowManager::WindowManager()");
  DLOG_TRACE("--WindowManager::WindowManager()");
}

/**
 * @brief Destructor for the WindowManager class.
 *
 * This destructor stops rendering frames for all windows controlled by the
 * WindowManager. It calls the stop_frame_callbacks() function to stop rendering
 * frames.
 */
WindowManager::~WindowManager() {
  DLOG_TRACE("++WindowManager::~WindowManager()");
  DLOG_TRACE("--WindowManager::~WindowManager()");
}

/**
 * @brief Dispatches events from the Wayland display.
 *
 * This function dispatches events from the Wayland display with a specified
 * timeout.
 *
 * @param timeout The maximum amount of time to wait for events, in
 * milliseconds.
 * @return The number of events dispatched on success, or a negative error code
 * on failure.
 */
[[maybe_unused]] int WindowManager::dispatch(const int timeout) const {
  pollfd fds[1];
  int dispatch_count = 0;

  while (g_main_context_iteration(nullptr, FALSE))
    ;

  while (wl_display_prepare_read(get_display()) != 0)
    dispatch_count += wl_display_dispatch_pending(get_display());

  if (wl_display_flush(get_display()) < 0 && errno != EAGAIN) {
    wl_display_cancel_read(get_display());
    return -errno;
  }

  fds[0] = {
      .fd = wl_display_get_fd(get_display()),
      .events = POLLIN,
      .revents = 0,
  };

  const int ret = poll(fds, std::size(fds), timeout);
  if (ret > 0) {
    if (fds[0].revents & POLLIN) {
      wl_display_read_events(get_display());
      dispatch_count += wl_display_dispatch_pending(get_display());
      return dispatch_count;
    }

    wl_display_cancel_read(get_display());
    return dispatch_count;
  }
  if (ret == 0) {
    wl_display_cancel_read(get_display());
    return dispatch_count;
  }
  wl_display_cancel_read(get_display());
  return -errno;
}

/**
 * @class WindowManager
 * @brief Class for managing windows and handling event polling
 *
 * The WindowManager class provides functionality for managing windows and
 * handling events using Wayland protocol.
 */
int WindowManager::poll_events(int /* timeout */) const {
  for (const auto observer : observers_) {
    observer->notify_task();
  }

  while (wl_display_prepare_read(get_display()) != 0) {
    wl_display_dispatch_pending(get_display());
  }
  wl_display_flush(get_display());

  wl_display_read_events(get_display());
  return wl_display_dispatch_pending(get_display());
}

int WindowManager::display_dispatch() const {
  return wl_display_dispatch(get_display());
}

wl_output* WindowManager::get_primary_output() const {
  auto& outputs = get_outputs();

  if (get_xdg_output_manager()) {
    for (const auto& [fst, snd] : outputs) {
      if (snd->get_xdg_output()->is_origin()) {
        LOG_DEBUG("get_primary_output: (xdg_output) Origin: {}", fmt::ptr(fst));
        return fst;
      }
    }
  } else {
    for (const auto& [fst, snd] : outputs) {
      LOG_DEBUG("get_primary_output: (fist) Origin: {}", fmt::ptr(fst));
      return fst;
    }
  }

  LOG_DEBUG("get_primary_output: (nullptr)");
  return nullptr;
}

wl_output* WindowManager::find_output_by_name(
    const std::string& output_name) const {
  auto& outputs = get_outputs();

  if (get_xdg_output_manager()) {
    for (const auto& [fst, snd] : outputs) {
      if (snd->get_name() == output_name) {
        LOG_DEBUG("find_output_by_name: (xdg_output): {}", output_name);
        return fst;
      }
    }
  } else {
    for (const auto& [fst, snd] : outputs) {
      LOG_DEBUG("find_output_by_name: (fist): {}", output_name);
      return fst;
    }
  }
  return nullptr;
}
