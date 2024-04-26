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

#include "xdg_window_manager.h"

#include "logging.h"
#include "window/xdg_toplevel.h"


/**
 * @class XdgWindowManager
 *
 * @brief XdgWm represents a surface manager for a Wayland-based display.
 *
 * The XdgWm class is responsible for managing application windows using the XDG Shell protocol.
 */
XdgWindowManager::XdgWindowManager(Keyboard::KeyCallback keyboard_callback,
                                   const unsigned long ext_interface_count,
                                   const Registrar::RegistrarCallback *ext_interface_data,
                                   GMainContext *context,
                                   bool enable_cursor,
                                   const char *display_name) : WindowManager(keyboard_callback, ext_interface_count,
                                                                             ext_interface_data,
                                                                             context, enable_cursor, display_name) {
    SPDLOG_TRACE("++XdgWindowManager::XdgWindowManager()");
    auto xdg_wm_base = get_xdg_wm_base();
    if (!xdg_wm_base.has_value()) {
        spdlog::critical("XDG Window Manager is not supported.");
        exit(EXIT_FAILURE);
    }
    xdg_wm_base_ = xdg_wm_base.value();

    xdg_wm_base_add_listener(xdg_wm_base.value(), &xdg_wm_base_listener_, this);
    SPDLOG_TRACE("--XdgWindowManager::XdgWindowManager()");
}

/**
 * @class XdgWindowManager
 *
 * @brief The XdgWm class represents a Wayland shell surface manager.
 *
 * The XdgWm class manages the creation, destruction, configuration, and behavior of a Wayland shell surface manager.
 *
 * It is responsible for handling interactions with the Wayland registry, creating and destroying the surface manager base,
 * surface, and toplevel objects, and implementing the necessary event handling functions.
 */
XdgWindowManager::~XdgWindowManager() = default;

/**
 *
 * @param data
 * @param xdg_wm_base
 * @param serial
 */
void XdgWindowManager::xdg_wm_base_ping(void *data,
                                        struct xdg_wm_base *xdg_wm_base,
                                        uint32_t serial) {
    auto wm = static_cast<XdgWindowManager *>(data);
    if (wm->get_xdg_wm_base().value() != xdg_wm_base) {
        SPDLOG_CRITICAL("wm->get_xdg_wm_base().value() != xdg_wm_base");
        return;
    }
    SPDLOG_DEBUG("xdg_wm_base_ping");
    xdg_wm_base_pong(xdg_wm_base, serial);
}

XdgTopLevel *
XdgWindowManager::create_top_level(const char *title, const char *app_id, int width, int height, int buffer_count,
                                   uint32_t buffer_format,
                                   bool fullscreen, bool maximized, bool fullscreen_ratio, bool tearing,
                                   const std::function<void(void *, const uint32_t)> &frame_callback,
                                   const int32_t *context_attribs, size_t context_attribs_size,
                                   const int32_t *config_attribs, size_t config_attribs_size,
                                   int buffer_bpp, int swap_interval) {
    auto wm = reinterpret_cast<WindowManager *>(this);
    xdg_top_level_ = std::make_unique<XdgTopLevel>(wm, title, app_id, width, height, buffer_count, buffer_format,
                                                   fullscreen, maximized, fullscreen_ratio, tearing,
                                                   frame_callback, buffer_bpp, swap_interval,
                                                   context_attribs, context_attribs_size,
                                                   config_attribs, config_attribs_size);
    return xdg_top_level_.get();
}
