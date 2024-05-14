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

#include "window_manager.h"

#include <poll.h>
#include <wayland-client.h>

#include "logging.h"
#include "registrar.h"

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
WindowManager::WindowManager(
        struct wl_display *display,
        bool disable_cursor,
        const unsigned long ext_interface_count,
        const Registrar::RegistrarCallback *ext_interface_data,
        GMainContext *context)
        : Registrar(display,
                    ext_interface_count,
                    ext_interface_data,
                    disable_cursor),
          wl_display_(display),
          context_(context),
          outputs_(get_outputs()) {
    SPDLOG_TRACE("++WindowManager::WindowManager()");
    SPDLOG_TRACE("--WindowManager::WindowManager()");
}

/**
 * @brief Destructor for the WindowManager class.
 *
 * This destructor stops rendering frames for all windows controlled by the
 * WindowManager. It calls the stop_frame_callbacks() function to stop rendering
 * frames.
 */
WindowManager::~WindowManager() {
    SPDLOG_TRACE("++WindowManager::~WindowManager()");
    SPDLOG_TRACE("--WindowManager::~WindowManager()");
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
[[maybe_unused]] int WindowManager::dispatch(int timeout) const {
    struct pollfd fds[1];
    int dispatch_count = 0;

    while (g_main_context_iteration(nullptr, FALSE));

    while (wl_display_prepare_read(wl_display_) != 0)
        dispatch_count += wl_display_dispatch_pending(wl_display_);

    if (wl_display_flush(wl_display_) < 0 && errno != EAGAIN) {
        wl_display_cancel_read(wl_display_);
        return -errno;
    }

    fds[0] = (struct pollfd) {wl_display_get_fd(wl_display_), POLLIN};

    const int ret = poll(fds, std::size(fds), timeout);
    if (ret > 0) {
        if (fds[0].revents & POLLIN) {
            wl_display_read_events(wl_display_);
            dispatch_count += wl_display_dispatch_pending(wl_display_);
            return dispatch_count;
        } else {
            wl_display_cancel_read(wl_display_);
            return dispatch_count;
        }
    } else if (ret == 0) {
        wl_display_cancel_read(wl_display_);
        return dispatch_count;
    } else {
        wl_display_cancel_read(wl_display_);
        return -errno;
    }
}

/**
 * @class WindowManager
 * @brief Class for managing windows and handling event polling
 *
 * The WindowManager class provides functionality for managing windows and
 * handling events using Wayland protocol.
 */
int WindowManager::poll_events(int /* timeout */) const {
    for (auto observer: observers_) {
        observer->notify_task();
    }

    while (wl_display_prepare_read(wl_display_) != 0) {
        wl_display_dispatch_pending(wl_display_);
    }
    wl_display_flush(wl_display_);

    wl_display_read_events(wl_display_);
    return wl_display_dispatch_pending(wl_display_);
}

int WindowManager::display_dispatch() const {
    return wl_display_dispatch(wl_display_);
}

struct wl_output *WindowManager::get_primary_output() {
    auto &outputs = get_outputs();

    if (get_xdg_output_manager()) {
        for (auto &output: outputs) {
            if (output.second->get_xdg_output()->is_origin()) {
                spdlog::debug("get_primary_output: (xdg_output) Origin: {}",
                              fmt::ptr(output.first));
                return output.first;
            }
        }
    } else {
        for (auto &output: outputs) {
            spdlog::debug("get_primary_output: (fist) Origin: {}",
                          fmt::ptr(output.first));
            return output.first;
        }
    }

    spdlog::debug("get_primary_output: (nullptr)");
    return nullptr;
}

struct wl_output *WindowManager::find_output_by_name(
        const std::string &output_name) {
    auto &outputs = get_outputs();

    if (get_xdg_output_manager()) {
        for (auto &output: outputs) {
            if (output.second->get_name() == output_name) {
                spdlog::debug("find_output_by_name: (xdg_output): {}", output_name);
                return output.first;
            }
        }
    } else {
        for (auto &output: outputs) {
            spdlog::debug("find_output_by_name: (fist): {}", output_name);
            return output.first;
        }
    }
    return nullptr;
}
