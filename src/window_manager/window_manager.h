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

#ifndef SRC_WINDOW_WINDOW_MANAGER_H_
#define SRC_WINDOW_WINDOW_MANAGER_H_

#include "display.h"

#include <list>

#include "window/window.h"
#include "window/window_egl.h"

#include "xdg_wm.h"


class Display;

class WindowEgl;

class Window;

class WindowManager : public Display, public Window {
public:
    typedef enum {
        EGL,
        VULKAN,
    } WindowType;

    explicit WindowManager(Window::ShellType shell_type = Window::ShellType::XDG, GMainContext *context = nullptr,
                           bool enable_cursor = true,
                           const char *name = nullptr);

    ~WindowManager() override;

    WindowEgl *
    create_window(int width, int height, WindowType window_type = WindowType::EGL,
                  const std::function<void(void *data, uint32_t time)> &draw_callback = nullptr);

    [[nodiscard]] int poll_events(int timeout) const;

    [[nodiscard]] int dispatch(int timeout) const;

private:
    // list of windows for z-order control
    std::list<std::unique_ptr<WindowEgl>> windows_;
    std::unique_ptr<XdgWm> xdg_wm_;

    Window::ShellType shell_type_;

    static void handle_surface_enter(void *data,
                                     struct wl_surface *surface,
                                     struct wl_output *output);

    static void handle_surface_leave(void *data,
                                     struct wl_surface *surface,
                                     struct wl_output *output);

    static const struct wl_surface_listener surface_listener_;
};

#endif // SRC_WINDOW_WINDOW_MANAGER_H_