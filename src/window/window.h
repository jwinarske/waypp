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

#ifndef SRC_WINDOW_WINDOW_H_
#define SRC_WINDOW_WINDOW_H_

#include <functional>

#include <wayland-client.h>

class Display;

class Window {
public:
    typedef enum {
        AGL,
        IVI,
        XDG,
        NONE,
    } ShellType;

    explicit Window(struct wl_compositor *compositor, ShellType shell_type = XDG,
                    const std::function<void(void *data, uint32_t time)> &draw_callback = nullptr);

    virtual ~Window() = 0;

    friend class WindowEgl;

    friend class WindowVulkan;

    friend class WindowManager;

private:
    struct wl_surface *wl_surface_{};
    struct wl_callback *wl_callback_{};

    ShellType shell_type_;

    std::function<void(void *data, uint32_t time)> draw_callback_;

    void start_frames();

    void stop_frames() const;

    static void on_frame(void *data, struct wl_callback *callback, uint32_t time);

    static const struct wl_callback_listener frame_listener_;
};

#endif // SRC_WINDOW_WINDOW_H_