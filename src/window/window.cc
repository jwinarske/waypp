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

#include "window.h"

#include <wayland-client.h>

/**
 * @class Window
 * @brief Class representing a window in a compositor-based system
 *
 * This class represents a window in a compositor-based system. It provides
 * methods for creating and managing a surface, as well as starting and
 * stopping frame updates.
 */
Window::Window(struct wl_compositor *compositor, ShellType shell_type,
               const std::function<void(void *data, uint32_t time)> &draw_callback) :
        shell_type_(shell_type),
        draw_callback_(draw_callback) {
    wl_surface_ = wl_compositor_create_surface(compositor);
    start_frames();
}

/**
 * @brief Destructor for the Window class.
 *
 * This destructor stops any active frames by calling the `stop_frames()` function.
 * It is responsible for destroying the `wl_callback` if it exists.
 *
 * @see stop_frames()
 */
Window::~Window() {
    stop_frames();
}

/**
 * @brief Start rendering frames for the window.
 *
 * This function stops the current frames (if any) and starts rendering new frames by calling the `on_frame` function.
 *
 * @note This function assumes that the window has been initialized properly.
 */
void Window::start_frames() {
    stop_frames();
    on_frame(this, nullptr, 0);
}

/**
 * Stops the frame rendering by destroying the wl_callback object if it exists.
 * This function is intended to be called from outside the Window class.
 */
void Window::stop_frames() const {
    if (wl_callback_) {
        wl_callback_destroy(wl_callback_);
    }
}

/**
 * @brief Callback function for frame completion event
 *
 * This function is called when a frame completion event is received.
 * It updates the state of the Window object and invokes the draw_callback_ function.
 *
 * @param data Pointer to the Window object
 * @param callback Pointer to the wl_callback object (unused)
 * @param time Timestamp of the frame completion event
 */
void Window::on_frame(void *data,
                      struct wl_callback *callback,
                      const uint32_t time) {
    const auto obj = static_cast<Window *>(data);

    obj->wl_callback_ = nullptr;

    if (callback) {
        wl_callback_destroy(callback);
    }

    if (obj->draw_callback_) {
        obj->draw_callback_(data, time);
    }

    obj->wl_callback_ = wl_surface_frame(obj->wl_surface_);
    wl_callback_add_listener(obj->wl_callback_, &Window::frame_listener_, data);

    wl_surface_commit(obj->wl_surface_);
}

const struct wl_callback_listener Window::frame_listener_ = {
        .done = on_frame
};
