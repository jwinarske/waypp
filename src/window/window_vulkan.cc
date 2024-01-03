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

#include "window_vulkan.h"

/**
 * @class WindowVulkan
 * @brief Class representing a Vulkan window.
 *
 * This class is a specialized subclass of Window that is specifically designed for Vulkan rendering.
 * It provides functionality to create and manage a Vulkan surface.
 */
WindowVulkan::WindowVulkan(struct wl_display *display, struct wl_compositor *compositor, int width, int height,
                           Window::ShellType shell_type, const std::function<void(void *, uint32_t)> &draw_callback) :
        Window(compositor, shell_type, draw_callback) {
}

/**
 * @class WindowVulkan
 * @brief Class representing a Vulkan window
 *
 * The WindowVulkan class provides functionality to create and manage a Vulkan window.
 * It is primarily used for rendering graphics using the Vulkan API.
 */
WindowVulkan::~WindowVulkan() = default;