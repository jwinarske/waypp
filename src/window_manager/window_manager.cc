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
 * @brief The WindowManager class is responsible for managing windows and handling surface-related operations.
 *
 * The WindowManager class extends the Display and Window classes and is used to create and manage windows in a graphical user interface application.
 *
 * @see Display
 * @see Window
 * @see XdgWm
 */
WindowManager::WindowManager(GMainContext *context,
                             bool enable_cursor,
                             const char *display_name) : Registrar(get_display(display_name)),
                                                         context_(context),
                                                         outputs_(get_outputs()),
                                                         cursor_{.enable = enable_cursor} {
    SPDLOG_TRACE("++WindowManager::WindowManager()");
    if (enable_cursor) {
        if (!shm_has_format(WL_SHM_FORMAT_XRGB8888)) {
            spdlog::warn("XRGB is not supported. Disabling cursor");
            cursor_.enable = false;
            return;
        }
        cursor_.cursor = std::make_unique<Cursor>(get_shm().value(), get_compositor());
    }
    //    start_frame_callbacks();
    SPDLOG_TRACE("--WindowManager::WindowManager()");
}

/**
 * @brief Destructor for the WindowManager class.
 *
 * This destructor stops rendering frames for all windows controlled by the WindowManager.
 * It calls the stop_frame_callbacks() function to stop rendering frames.
 */
WindowManager::~WindowManager() {
    SPDLOG_TRACE("++WindowManager::~WindowManager()");
    wl_display_flush(wl_display_);
    wl_display_disconnect(wl_display_);
    SPDLOG_TRACE("--WindowManager::~WindowManager()");
}

struct wl_display *WindowManager::get_display(const char *name) {
    SPDLOG_TRACE("++WindowManager::get_display()");
    wl_display_ = (wl_display_connect(name));
    if (wl_display_ == nullptr) {
        spdlog::critical("Failed to connect to Wayland display. {}", strerror(errno));
        exit(EXIT_FAILURE);
    }
    SPDLOG_TRACE("--WindowManager::get_display()");
    return wl_display_;
}

/**
 * @brief Dispatches events from the Wayland display.
 *
 * This function dispatches events from the Wayland display with a specified timeout.
 *
 * @param timeout The maximum amount of time to wait for events, in milliseconds.
 * @return The number of events dispatched on success, or a negative error code on failure.
 */
[[maybe_unused]] int WindowManager::dispatch(int timeout) const {
    struct pollfd fds[1];
    int dispatch_count = 0;

    while (g_main_context_iteration(nullptr, FALSE));

    while (wl_display_prepare_read(wl_display_) != 0)
        dispatch_count += wl_display_dispatch_pending(wl_display_);

    if (wl_display_flush(wl_display_) < 0 &&
        errno != EAGAIN) {
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
 * The WindowManager class provides functionality for managing windows and handling events using Wayland protocol.
 */
int WindowManager::poll_events(int /* timeout */) const {
    while (wl_display_prepare_read(wl_display_) != 0) {
        wl_display_dispatch_pending(wl_display_);
    }
    wl_display_flush(wl_display_);

    wl_display_read_events(wl_display_);
    return wl_display_dispatch_pending(wl_display_);
}
