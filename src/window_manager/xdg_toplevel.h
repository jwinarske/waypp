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
                struct wl_compositor *wl_compositor,
                const std::optional<struct wp_viewporter *> &viewporter,
                const std::optional<struct wp_fractional_scale_manager_v1 *> &fractional_scale_manager,
                const std::optional<struct wp_tearing_control_manager_v1 *> &tearing_control_manager,
                const std::map<struct wl_output *, std::unique_ptr<Output>> &outputs,
                const char *name, int width, int height, wl_output_transform buffer_transform,
                bool fullscreen, bool maximized, bool fullscreen_ratio, bool tearing,
                const std::function<void(void *, const uint32_t)> &draw_frame_callback,
                const int32_t *context_attribs = nullptr, size_t context_attribs_size = 0,
                const int32_t *config_attribs = nullptr, size_t config_attribs_size = 0,
                int buffer_bpp = 0, int swap_interval = 0);

    ~XdgTopLevel();

    void set_app_id(const char *app_id) { xdg_toplevel_set_app_id(xdg_toplevel_, app_id); }

    void set_title(const char *title) { xdg_toplevel_set_title(xdg_toplevel_, title); }

    void set_fullscreen() { xdg_toplevel_set_fullscreen(xdg_toplevel_, nullptr); }

    void set_minimize() { xdg_toplevel_set_minimized(xdg_toplevel_); }

    void set_maximize() { xdg_toplevel_set_maximized(xdg_toplevel_); }

    void resize(int width, int height);

private:
    WindowManager *wm_;
    struct xdg_surface *xdg_surface_;
    struct xdg_toplevel *xdg_toplevel_;

    std::string title_;
    std::string app_id_;

    volatile bool wait_for_configure_;

    bool fullscreen_{};
    bool maximized_{};
    bool fullscreen_ratio_{};

    bool resize_{};
    bool activated_{};
    bool running_{};

    struct {
        int32_t width;
        int32_t height;
    } window_size_{};

    struct {
        int width;
        int height;
    } buffer_size_{};

    struct {
        int width;
        int height;
    } logical_size_{};

    static void handle_xdg_surface_configure(
            void *data,
            struct xdg_surface *xdg_surface,
            uint32_t serial);

    static constexpr struct xdg_surface_listener xdg_surface_listener_ = {
            .configure = handle_xdg_surface_configure,
    };

    static void handle_toplevel_configure(
            void *data,
            struct xdg_toplevel *toplevel,
            int32_t width,
            int32_t height,
            struct wl_array *states);

    static void handle_toplevel_close(
            void *data,
            struct xdg_toplevel *xdg_toplevel);

#if defined(XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION)

    static void handle_configure_bounds(void *data,
                                        struct xdg_toplevel *xdg_toplevel,
                                        int32_t width,
                                        int32_t height);

#endif
#if defined(XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)

    static void handle_wm_capabilities(void *data,
                                       struct xdg_toplevel *xdg_toplevel,
                                       struct wl_array *capabilities);

#endif

    static constexpr struct xdg_toplevel_listener xdg_toplevel_listener_ = {
            .configure = handle_toplevel_configure,
            .close = handle_toplevel_close
#if defined(XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION)
            ,
            .configure_bounds = handle_configure_bounds
#endif
#if defined(XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
            ,
            .wm_capabilities = handle_wm_capabilities
#endif
    };
};
