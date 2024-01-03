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

#include "xdg_wm.h"

#include <cstring>
#include <iostream>

// workaround for Wayland macro not compiling in C++
#define WL_ARRAY_FOR_EACH(pos, array, type)                             \
  for (pos = (type)(array)->data;                                       \
       (const char*)pos < ((const char*)(array)->data + (array)->size); \
       (pos)++)


/**
 * @class XdgWm
 *
 * @brief XdgWm represents a window manager for a Wayland-based display.
 *
 * The XdgWm class is responsible for managing application windows using the XDG Shell protocol.
 */
XdgWm::XdgWm(struct wl_display *display, struct wl_surface *base_surface) : wl_surface_(base_surface) {
    wl_registry_ = wl_display_get_registry(display);
    wl_registry_add_listener(wl_registry_, &registry_listener_, this);

    // enables blocking caller until set false
    wait_for_configure_ = true;
}

/**
 * @class XdgWm
 *
 * @brief The XdgWm class represents a Wayland shell window manager.
 *
 * The XdgWm class manages the creation, destruction, configuration, and behavior of a Wayland shell window manager.
 *
 * It is responsible for handling interactions with the Wayland registry, creating and destroying the window manager base,
 * surface, and toplevel objects, and implementing the necessary event handling functions.
 */
XdgWm::~XdgWm() {
    if (wl_registry_)
        wl_registry_destroy(wl_registry_);

    if (xdg_toplevel_)
        xdg_toplevel_destroy(xdg_toplevel_);

    if (xdg_surface_)
        xdg_surface_destroy(xdg_surface_);

    if (xdg_wm_base_)
        xdg_wm_base_destroy(xdg_wm_base_);
}

/**
 * @brief Handles global registry events.
 *
 * This function is called when a new global object is added or removed from the registry.
 * If the added global object is of type xdg_wm_base, it sets up the necessary listeners
 * and binds the object to the XdgWm instance.
 *
 * @param data The user data associated with the callback (XdgWm instance).
 * @param registry The registry object.
 * @param name The name of the global object.
 * @param interface The interface name of the global object.
 * @param version The version of the global object.
 */
void XdgWm::registry_handle_global(void *data,
                                   struct wl_registry *registry,
                                   uint32_t name,
                                   const char *interface,
                                   uint32_t version) {

    if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        const auto obj = static_cast<XdgWm *>(data);

        obj->xdg_wm_base_ = static_cast<struct xdg_wm_base *>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface,
                                 std::min(static_cast<uint32_t>(3), version)));
        xdg_wm_base_add_listener(obj->xdg_wm_base_, &xdg_wm_base_listener_, obj);

        obj->xdg_surface_ = xdg_wm_base_get_xdg_surface(obj->xdg_wm_base_, obj->wl_surface_);
        xdg_surface_add_listener(obj->xdg_surface_, &xdg_surface_listener_, obj);

        obj->xdg_toplevel_ = xdg_surface_get_toplevel(obj->xdg_surface_);
        xdg_toplevel_add_listener(obj->xdg_toplevel_, &xdg_toplevel_listener_, obj);

        xdg_toplevel_set_title(obj->xdg_toplevel_, "waypp");
        xdg_toplevel_set_app_id(obj->xdg_toplevel_, "waypp");

        wl_surface_commit(obj->wl_surface_);
    }
}

/**
 * @brief Handles the removal of a global from the registry.
 *
 * This function is a callback that is triggered when a global object is
 * removed from the registry. It does not return anything.
 *
 * @param data     A pointer to user-defined data.
 * @param reg      A pointer to the registry that triggered the event.
 * @param id       The ID of the removed global.
 */
void XdgWm::registry_handle_global_remove(void * /* data */,
                                          struct wl_registry * /* reg */,
                                          uint32_t /* id */) {
}

const struct wl_registry_listener XdgWm::registry_listener_ = {
        registry_handle_global,
        registry_handle_global_remove,
};

/**
 * @class XdgWm
 *
 * The XdgWm class represents a window manager for XDG surfaces.
 */
void XdgWm::xdg_wm_base_ping(void * /* data */,
                             struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial) {
    std::cout << "XdgWm::xdg_wm_base_ping" << std::endl;
    xdg_wm_base_pong(xdg_wm_base, serial);
}

const struct xdg_wm_base_listener XdgWm::xdg_wm_base_listener_ = {
        .ping = xdg_wm_base_ping,
};

/**
 * @brief Handles the configure event for xdg_surface.
 *
 * This function is a member function of the XdgWm class. It is called when the xdg_surface
 * sends a configure event. It acknowledges the configure request by calling xdg_surface_ack_configure().
 * It also sets the wait_for_configure_ variable to false.
 *
 * @param data A pointer to the XdgWm instance.
 * @param xdg_surface A pointer to the xdg_surface instance.
 * @param serial The serial number of the configure event.
 */
void XdgWm::handle_xdg_surface_configure(
        void *data,
        struct xdg_surface *xdg_surface,
        uint32_t serial) {
    auto *w = static_cast<XdgWm *>(data);
    xdg_surface_ack_configure(xdg_surface, serial);
    w->wait_for_configure_ = false;
}

const struct xdg_surface_listener XdgWm::xdg_surface_listener_ = {
        .configure = handle_xdg_surface_configure};

/**
 * @brief Handles the configure event for a toplevel surface.
 *
 * This function is called when the configure event is received for a toplevel surface.
 * It updates the internal state of the XdgWm object based on the configuration properties
 * received from the compositor.
 *
 * @param data The user data passed to the callback.
 * @param toplevel The toplevel surface that triggered the event.
 * @param width The width of the surface.
 * @param height The height of the surface.
 * @param states An array of states associated with the surface.
 */
void XdgWm::handle_toplevel_configure(
        void *data,
        struct xdg_toplevel * /* toplevel */,
        int32_t width,
        int32_t height,
        struct wl_array *states) {

    if (width == 0 || height == 0) {
        // Compositor is deferring to us
        return;
    }

    auto *w = static_cast<XdgWm *>(data);

    w->fullscreen_ = false;
    w->maximized_ = false;
    w->resize_ = false;
    w->activated_ = false;

    const uint32_t *state;
    WL_ARRAY_FOR_EACH(state, states, const uint32_t*) {
        switch (*state) {
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                std::cout << "XDG_TOPLEVEL_STATE_FULLSCREEN" << std::endl;
                w->fullscreen_ = true;
                break;
            case XDG_TOPLEVEL_STATE_MAXIMIZED:
                std::cout << "XDG_TOPLEVEL_STATE_MAXIMIZED" << std::endl;
                w->maximized_ = true;
                break;
            case XDG_TOPLEVEL_STATE_RESIZING:
                std::cout << "XDG_TOPLEVEL_STATE_RESIZING" << std::endl;
                w->resize_ = true;
                break;
            case XDG_TOPLEVEL_STATE_ACTIVATED:
                std::cout << "XDG_TOPLEVEL_STATE_ACTIVATED" << std::endl;
                w->activated_ = true;
                break;
        }
    }

    if (width > 0 && height > 0) {
        if (!w->fullscreen_ && !w->maximized_) {
            w->window_size_.width = width;
            w->window_size_.height = height;
        }
        w->geometry_.width = width;
        w->geometry_.height = height;

    } else if (!w->fullscreen_ && !w->maximized_) {
        w->geometry_.width = w->window_size_.width;
        w->geometry_.height = w->window_size_.height;
    }
    std::cout << "width: " << width << std::endl;
    std::cout << "height: " << height << std::endl;
}

/**
 * @brief Handles the close event of a toplevel window.
 *
 * This function is a callback that is called when the user requests to close
 * the toplevel window. It sets the `running_` member variable to false, which
 * will cause the main event loop to exit.
 *
 * @param data The user data associated with the XdgWm instance.
 * @param xdg_toplevel The xdg_toplevel object that received the close request.
 */
void XdgWm::handle_toplevel_close(
        void *data,
        struct xdg_toplevel * /* xdg_toplevel */) {
    std::cout << "XdgWm::handle_toplevel_close" << std::endl;

    auto *w = static_cast<XdgWm *>(data);
    w->running_ = false;
}

const struct xdg_toplevel_listener XdgWm::xdg_toplevel_listener_ = {
        handle_toplevel_configure,
        handle_toplevel_close,
};

/**
*
*/
void XdgWm::toplevel_resize(int x, int y, int width, int height, int padding) {

    const bool top = y < padding;
    const bool bottom = y > (height - padding);
    const bool left = x < padding;
    const bool right = x > (width - padding);

    auto edge = XDG_TOPLEVEL_RESIZE_EDGE_NONE;

    if (top)
        if (right)
            edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
        else if (left)
            edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
        else
            edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP;
    else if (bottom)
        if (right)
            edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
        else if (left)
            edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
        else
            edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
    else if (right)
        edge = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
    else if (left)
        edge = XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
    else
        edge = XDG_TOPLEVEL_RESIZE_EDGE_NONE;

    if (edge) {
#if 0
        xdg_toplevel_resize(
                xdg_toplevel_,
                wl_seat_,
                0,
                edge);
#endif
    }
}