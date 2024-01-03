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

#ifndef SRC_WINDOW_WINDOW_VULKAN_H_
#define SRC_WINDOW_WINDOW_VULKAN_H_

#include "window.h"


class WindowVulkan : public Window {
public:
    explicit WindowVulkan(struct wl_display *display, struct wl_compositor *compositor, int width, int height,
                          ShellType shell_type = XDG,
                          const std::function<void(void *data, uint32_t time)> &draw_callback = nullptr);

    ~WindowVulkan() override;

private:
};

#endif // SRC_WINDOW_WINDOW_VULKAN_H_