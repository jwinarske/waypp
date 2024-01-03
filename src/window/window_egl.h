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

#ifndef SRC_WINDOW_WINDOW_EGL_H_
#define SRC_WINDOW_WINDOW_EGL_H_

#include "window.h"
#include "egl.h"

class WindowEgl : public Egl {
public:
    explicit WindowEgl(struct wl_display *display, struct wl_compositor *compositor, struct wl_surface *surface,
                       int width, int height,
                       Window::ShellType shell_type = Window::ShellType::XDG,
                       const std::function<void(void *data, uint32_t time)> &draw_callback = nullptr);

    ~WindowEgl();

    friend class Egl;

private:
    struct wl_egl_window *egl_window_{};
};

#endif // SRC_WINDOW_WINDOW_EGL_H_