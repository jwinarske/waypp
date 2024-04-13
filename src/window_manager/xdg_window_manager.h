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

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "wayland-protocols.h"

#include "window_manager.h"

class XdgTopLevel;

class XdgWindowManager : public WindowManager {
public:
    explicit XdgWindowManager(GMainContext *context = nullptr,
                              bool enable_cursor = true,
                              const char *display_name = nullptr);

    ~XdgWindowManager();

    XdgTopLevel *CreateTopLevel(const char *name, int width, int height, enum wl_output_transform buffer_transform,
                                bool fullscreen, bool maximized, bool fullscreen_ratio, bool tearing,
                                const std::function<void(void *, const uint32_t)> &draw_frame_callback,
                                const int32_t *context_attribs = nullptr, size_t context_attribs_size = 0,
                                const int32_t *config_attribs = nullptr, size_t config_attribs_size = 0,
                                int buffer_bpp = 0, int swap_interval = 0);

    // Disallow copy and assign.
    XdgWindowManager(const XdgWindowManager &) = delete;

    XdgWindowManager &operator=(const XdgWindowManager &) = delete;

private:
    std::unique_ptr<XdgTopLevel> xdg_top_level_;

    static void xdg_wm_base_ping(void *data,
                                 struct xdg_wm_base *xdg_wm_base,
                                 uint32_t serial);

    static constexpr struct xdg_wm_base_listener xdg_wm_base_listener_ = {
            .ping = xdg_wm_base_ping,
    };
};
