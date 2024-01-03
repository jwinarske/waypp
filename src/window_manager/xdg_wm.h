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

#ifndef SRC_WINDOW_MANAGER_XDG_WM_H_
#define SRC_WINDOW_MANAGER_XDG_WM_H_

#include <cstdint>
#include <string>

#include "xdg-shell-client-protocol.h"

class XdgWm {
public:
    XdgWm(struct wl_display *display, struct wl_surface *base_surface);

    ~XdgWm();

    [[nodiscard]] const bool get_wait_for_configure() const { return wait_for_configure_; }

    void set_app_id(const char *app_id) { xdg_toplevel_set_app_id(xdg_toplevel_, app_id); }

    void set_title(const char *title) { xdg_toplevel_set_title(xdg_toplevel_, title); }

    void toplevel_resize(int x, int y, int width, int height, int padding);

private:
    struct wl_surface *wl_surface_;
    struct wl_registry *wl_registry_;
    struct xdg_wm_base *xdg_wm_base_{};
    struct xdg_surface *xdg_surface_{};
    struct xdg_toplevel *xdg_toplevel_{};

    std::string app_id_;

    volatile bool wait_for_configure_{};

    bool fullscreen_{};
    bool maximized_{};
    bool resize_{};
    bool activated_{};
    bool running_{};

    struct {
        int32_t width;
        int32_t height;
    } geometry_{};

    struct {
        uint32_t x;
        uint32_t y;
    } activation_area_{};

    struct {
        int32_t width;
        int32_t height;
    } window_size_{};

    static void handle_xdg_surface_configure(
            void * /* data */,
            struct xdg_surface * /* xdg_surface */,
            uint32_t /* serial */);

    static const struct xdg_surface_listener xdg_surface_listener_;

    static void handle_toplevel_configure(
            void * /* data */,
            struct xdg_toplevel * /* toplevel */,
            int32_t /* width */,
            int32_t /* height */,
            struct wl_array * /* states */);

    static void handle_toplevel_close(
            void * /* data */,
            struct xdg_toplevel * /* xdg_toplevel */);

    static const struct xdg_toplevel_listener xdg_toplevel_listener_;

    static void registry_handle_global(void *data,
                                       struct wl_registry *registry,
                                       uint32_t name,
                                       const char *interface,
                                       uint32_t version);

    static void registry_handle_global_remove(void *data,
                                              struct wl_registry *reg,
                                              uint32_t id);

    static const struct wl_registry_listener registry_listener_;

    static void xdg_wm_base_ping(void *data,
                                 struct xdg_wm_base *xdg_wm_base,
                                 uint32_t serial);

    static const struct xdg_wm_base_listener xdg_wm_base_listener_;

};

#endif // SRC_WINDOW_MANAGER_XDG_WM_H_