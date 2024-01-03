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

#include <iostream>

#include <poll.h>
#include <wayland-client.h>

#include "window/window_vulkan.h"


/**
 * @class WindowManager
 *
 * @brief The WindowManager class is responsible for managing windows and handling window-related operations.
 *
 * The WindowManager class extends the Display and Window classes and is used to create and manage windows in a graphical user interface application.
 *
 * @see Display
 * @see Window
 * @see XdgWm
 */
WindowManager::WindowManager(Window::ShellType shell_type, GMainContext *context, bool enable_cursor,
                             const char *name) :
        Display(context, enable_cursor, name),
        Window(wl_compositor_, shell_type,
               [&](void * /* data */, uint32_t /* time */) { std::cerr << "base draw" << std::endl; }),
        shell_type_(shell_type) {

    if (shell_type == XDG) {
        xdg_wm_ = std::make_unique<XdgWm>(this->wl_display_, this->wl_surface_);

        // this makes the start-up from the beginning with the correct dimensions
        // like starting as maximized/fullscreen, rather than starting up as floating
        // width, height then performing a resize
        while (xdg_wm_->get_wait_for_configure()) {
            wl_display_dispatch(wl_display_);

            // wait until xdg_surface::configure ACKs the new dimensions
            if (xdg_wm_->get_wait_for_configure())
                continue;
        }
        std::cout << "configured." << std::endl;
    }

    start_frames();
}

/**
 * @brief Destructor for the WindowManager class.
 *
 * This destructor stops rendering frames for all windows controlled by the WindowManager.
 * It calls the stop_frames() function to stop rendering frames.
 */
WindowManager::~WindowManager() {
    stop_frames();
}

/**

   * @brief Handles the event when a surface enters the window manager

   *
   * @param data The data associated with the event
   * @param surface The surface that enters the window manager
   * @param output The output associated with the surface
   *
   * This function is called when a surface enters the window manager. It prints a message indicating that the surface has entered.
   *
   * Example Usage:
   *
   * ```
   * WindowManager wm;
   * wl_surface *surface;
   * wl_output *output;
   * wm.handle_surface_enter(nullptr, surface, output);
   * ```
   *
   */
void WindowManager::handle_surface_enter(void * /* data */,
                                         struct wl_surface * /* surface */,
                                         struct wl_output * /* output */) {
    std::cout << "WindowManager::handle_surface_enter" << std::endl;
}

/**
 * @brief Handles the event when a surface leaves an output.
 *
 * This function is called when a surface leaves an output. It prints a message
 * to the console indicating that the surface has left the output.
 *
 * @param data    A pointer to the data associated with the event (unused).
 * @param surface The surface that has left the output.
 * @param output  The output that the surface has left.
 */
void WindowManager::handle_surface_leave(void * /* data */,
                                         struct wl_surface * /* surface */,
                                         struct wl_output * /* output */) {
    std::cout << "WindowManager::handle_surface_leave" << std::endl;
}

const struct wl_surface_listener WindowManager::surface_listener_ = {
        .enter = handle_surface_enter,
        .leave = handle_surface_leave,
};

/**
 * @brief Creates a new window and adds it to the WindowManager's list of windows.
 *
 * The function creates a new window based on the given parameters and adds it to the list of windows managed by the
 * WindowManager. The type of the window can be either EGL or VULKAN. If the window type is EGL, a WindowEgl object is
 * created using the provided display, compositor, surface, width, height, shell type, and draw callback. If the shell
 * type is XDG, additional actions can be performed. If the window type is VULKAN, a WindowVulkan object can be
 * created, but this part of the code is currently commented out.
 *
 * @param width The width of the window.
 * @param height The height of the window.
 * @param window_type The type of the window (EGL or VULKAN).
 * @param draw_callback The function to be called when the window needs to be drawn.
 * @return A pointer to the created window object, or nullptr if no window was created.
 */
WindowEgl *WindowManager::create_window(int width, int height, WindowType window_type,
                                        const std::function<void(void *data, uint32_t)> &draw_callback) {
    WindowEgl *result = nullptr;

    std::unique_ptr<WindowEgl> window;
    if (window_type == EGL) {
        window = std::make_unique<WindowEgl>(this->wl_display_, this->wl_compositor_, this->wl_surface_, width, height,
                                             shell_type_,
                                             draw_callback);
        if (shell_type_ == Window::ShellType::XDG) {
        }
    } else if (window_type == VULKAN) {
        //window = std::make_unique<WindowVulkan>(this->wl_display_, this->wl_compositor_, width, height, shell_type_,
        //draw_callback);
    }
    if (window) {
        result = window.get();
        windows_.emplace_back(std::move(window));
    }

    start_frames();
    return result;
}

/**
 * @brief Dispatches events from the Wayland display.
 *
 * This function dispatches events from the Wayland display with a specified timeout.
 *
 * @param timeout The maximum amount of time to wait for events, in milliseconds.
 * @return The number of events dispatched on success, or a negative error code on failure.
 */
int WindowManager::dispatch(int timeout) const {
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
