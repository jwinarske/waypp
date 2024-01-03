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

#include "window_egl.h"

#include <iostream>

#include <wayland-egl.h>

/**
 * @class WindowEgl
 * @brief The WindowEgl class represents a window using EGL for rendering.
 *
 * This class is responsible for managing an EGL window surface for rendering on a Wayland compositor. It creates an EGL window surface and initializes the EGL context. It also provides
* a callback for rendering the window contents.
 */
WindowEgl::WindowEgl(struct wl_display *display, struct wl_compositor * /* compositor */, struct wl_surface *surface,
                     int width, int height,
                     Window::ShellType /* shell_type */,
                     const std::function<void(void *data, uint32_t time)> & /* draw_callback */) :
        Egl(display) {

    std::cout << "width: " << width << std::endl;
    std::cout << "height: " << height << std::endl;

    if (egl_window_) {
        wl_egl_window_destroy(egl_window_);
    }
    egl_window_ = wl_egl_window_create(surface, width, height);

    if (egl_surface_) {
        eglDestroySurface(dpy_, egl_surface_);
    }

    auto create_platform_window =
            reinterpret_cast<PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC>(
                    eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT"));

    if (create_platform_window) {
        egl_surface_ = create_platform_window(dpy_, config_, egl_window_, nullptr);
    } else {
        egl_surface_ = eglCreateWindowSurface(
                dpy_, config_, reinterpret_cast<EGLNativeWindowType>(egl_window_), nullptr);
    }
}

/**
 * @class WindowEgl
 * @brief Represents an EGL window used by the application.
 *
 * This class manages the lifecycle of an EGL window created using the Wayland display protocol.
 * It provides functionality to destroy the EGL surface and associated resources.
 */
WindowEgl::~WindowEgl() {
    eglDestroySurface(dpy_, egl_surface_);

    if (egl_window_) {
        wl_egl_window_destroy(egl_window_);
    }
}
