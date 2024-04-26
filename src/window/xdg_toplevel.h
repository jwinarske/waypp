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
#include <map>
#include <memory>
#include <optional>

#include <wayland-client.h>

#include "window/window.h"
#include "window_manager/xdg_window_manager.h"

class Output;

class Window;

class XdgWindowManager;

class XdgTopLevel : public Window {
public:

    XdgTopLevel(WindowManager *wm,
                const char *title, const char *app_id, int width, int height, int buffer_count, uint32_t buffer_format,
                bool fullscreen, bool maximized, bool fullscreen_ratio, bool tearing,
                const std::function<void(void *, const uint32_t)> &frame_callback,
                int buffer_bpp = 0, int swap_interval = 0,
                const int32_t *context_attribs = nullptr, size_t context_attribs_size = 0,
                const int32_t *config_attribs = nullptr, size_t config_attribs_size = 0);

    ~XdgTopLevel();

    [[nodiscard]] uint32_t get_version() const { return xdg_toplevel_get_version(xdg_toplevel_); }

    [[nodiscard]] const std::string &get_app_id() const { return app_id_; }

    void set_app_id(const char *app_id) { xdg_toplevel_set_app_id(xdg_toplevel_, app_id); }

    [[nodiscard]] const std::string &get_title() const { return title_; }

    void set_title(const char *title) { xdg_toplevel_set_title(xdg_toplevel_, title); }

    [[nodiscard]] bool get_fullscreen() const { return fullscreen_; }

    void set_fullscreen() { xdg_toplevel_set_fullscreen(xdg_toplevel_, nullptr); }

    [[nodiscard]] bool get_maximize() const { return maximized_; }

    void set_maximize() { xdg_toplevel_set_maximized(xdg_toplevel_); }

    void set_minimize() { xdg_toplevel_set_minimized(xdg_toplevel_); }

    void resize(int width, int height);

    void set_surface_damage(int x, int y, int width, int height) {
        wl_surface_damage(wl_surface_, x, y, width, height);
    }


private:
    WindowManager *wm_;
    struct xdg_surface *xdg_surface_;
    struct xdg_toplevel *xdg_toplevel_;

    std::string title_;
    std::string app_id_;

    volatile bool wait_for_configure_;

    static void handle_xdg_surface_configure(
            void *data,
            struct xdg_surface *xdg_surface,
            uint32_t serial);

    static constexpr struct xdg_surface_listener xdg_surface_listener_ = {
            .configure = handle_xdg_surface_configure,
    };

    static void handle_xdg_toplevel_configure(
            void *data,
            struct xdg_toplevel *toplevel,
            int32_t width,
            int32_t height,
            struct wl_array *states);

    static void handle_xdg_toplevel_close(
            void *data,
            struct xdg_toplevel *xdg_toplevel);

#if defined(XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION)

    static void handle_xdg_toplevel_configure_bounds(void *data,
                                                     struct xdg_toplevel *xdg_toplevel,
                                                     int32_t width,
                                                     int32_t height);

#endif
#if defined(XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)

    static void handle_xdg_toplevel_wm_capabilities(void *data,
                                                    struct xdg_toplevel *xdg_toplevel,
                                                    struct wl_array *capabilities);

#endif

    static constexpr struct xdg_toplevel_listener xdg_toplevel_listener_ = {
            .configure = handle_xdg_toplevel_configure,
            .close = handle_xdg_toplevel_close
#if defined(XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION)
            ,
            .configure_bounds = handle_xdg_toplevel_configure_bounds
#endif
#if defined(XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
            ,
            .wm_capabilities = handle_xdg_toplevel_wm_capabilities
#endif
    };
};
