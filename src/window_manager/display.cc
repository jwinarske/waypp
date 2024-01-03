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

#include "display.h"
#include "output.h"

#include <iostream>
#include <cstring>
#include <cerrno>


/**
 * @class Display
 * Represents a Wayland display connection.
 */
Display::Display(GMainContext *context, bool enable_cursor, const char *name) :
        wl_display_(wl_display_connect(name)),
        context_(context),
        enable_cursor_(enable_cursor) {
    if (wl_display_ == nullptr) {
        std::cerr << "Failed to connect to Wayland display. " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
    wl_registry_ = wl_display_get_registry(wl_display_);
    wl_registry_add_listener(wl_registry_, &listener_, this);
    wl_display_roundtrip(wl_display_);
}

/**
 * @brief Destructor for the Display class.
 *
 * This destructor destroys the wl_registry_, wl_shm_, wl_subcompositor_, and wl_compositor_ objects if they exist.
 *
 * @note The Display::Display() function sets the pointer values of wl_registry_, wl_shm_, wl_subcompositor_, and wl_compositor_ to valid instances of their respective structures. This
* destructor handles the cleanup of these instances.
 *
 * @see Display(), wl_registry_destroy(), wl_shm_destroy(), wl_subcompositor_destroy(), wl_compositor_destroy()
 */
Display::~Display() {
    if (wl_registry_) {
        wl_registry_destroy(wl_registry_);
    }

    if (wl_shm_) {
        wl_shm_destroy(wl_shm_);
    }

    if (wl_subcompositor_) {
        wl_subcompositor_destroy(wl_subcompositor_);
    }

    if (wl_compositor_) {
        wl_compositor_destroy(wl_compositor_);
    }
}

/**
 * @class Display
 * @brief The Display class represents a Wayland display.
 *
 * This class provides functionality for interacting with a Wayland display,
 * such as managing various objects and handling events.
 */
void Display::shm_format(void *data,
                         struct wl_shm * /* wl_shm */,
                         uint32_t format) {
    const auto obj = static_cast<Display *>(data);
    if (format == WL_SHM_FORMAT_XRGB8888) {
        obj->has_xrgb_ = true;
    }
}

const struct wl_shm_listener Display::shm_listener_ = {
        .format = shm_format
};

/**
 * @brief Handles the global objects registered with the Wayland display.
 *
 * This method is called when a global object is added to the display registry. It checks
 * the interface of the object and binds it to the appropriate Wayland client-side object.
 *
 * @param data      A pointer to the Display object.
 * @param registry  The Wayland registry object.
 * @param name      The name of the global object.
 * @param interface The interface name of the global object.
 * @param version   The version of the global object.
 */
void Display::registry_handle_global(void *data,
                                     struct wl_registry *registry,
                                     uint32_t name,
                                     const char *interface,
                                     uint32_t version) {
    const auto obj = static_cast<Display *>(data);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        obj->compositor_version_ = version;
        obj->wl_compositor_ = static_cast<struct wl_compositor *>(
                wl_registry_bind(registry, name, &wl_compositor_interface,
                                 std::min(static_cast<uint32_t>(1), version)));
        obj->buffer_scaling_enabled_ = (obj->compositor_version_ >= 3);

    } else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
        obj->subcompositor_version_ = version;
        obj->wl_subcompositor_ = static_cast<struct wl_subcompositor *>(
                wl_registry_bind(registry, name, &wl_subcompositor_interface,
                                 std::min(static_cast<uint32_t>(1), version)));

    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        obj->wl_shm_ = static_cast<struct wl_shm *>(
                wl_registry_bind(registry, name, &wl_shm_interface,
                                 std::min(static_cast<uint32_t>(1), version)));
        wl_shm_add_listener(obj->wl_shm_, &shm_listener_, obj);

    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        auto output = static_cast<struct wl_output *>(
                wl_registry_bind(registry, name, &wl_output_interface,
                                 std::min(static_cast<uint32_t>(2), version)));
        obj->wl_outputs_[output] = std::make_unique<Output>(output, version);

    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        auto seat = static_cast<wl_seat *>(
                wl_registry_bind(registry, name, &wl_seat_interface,
                                 std::min(static_cast<uint32_t>(5), version)));
        obj->wl_seats_[seat] = std::make_unique<Seat>(seat, obj->wl_shm_, obj->wl_compositor_, obj->enable_cursor_,
                                                      version);
    }

    for (const auto &callback: obj->callbacks_) {
        callback.first(callback.second, registry, name, interface, version);
    }
}

/**
 * @brief Handles the removal of global objects from the registry.
 *
 * This function is invoked when a global object is removed from the registry.
 *
 * @param data Pointer to user data (unused).
 * @param reg Pointer to the registry object.
 * @param id The ID of the removed global object.
 *
 * @return None.
 */
void Display::registry_handle_global_remove(void * /* data */,
                                            struct wl_registry * /* reg */,
                                            uint32_t /* id */) {
}

const struct wl_registry_listener Display::listener_ = {
        registry_handle_global,
        registry_handle_global_remove,
};

/**
 * @brief Adds a registrar callback with associated data.
 *
 * This function allows you to register a callback function that will be called
 * when a new global object is added to the Wayland registry. The callback
 * function will be provided with the data parameter, the registry, the name of
 * the interface, and the version of the interface.
 *
 * @param callback The callback function to register.
 * @param data     The data to associate with the callback.
 *
 * @see wl_registry_add_listener
 * @see struct wl_registry_listener
 */
void Display::add_registrar_callback(const std::function<void(void *data, struct wl_registry *registry,
                                                              uint32_t name,
                                                              const char *interface,
                                                              uint32_t version)> &callback, void *data) {
    callbacks_.emplace_back(std::move(std::make_pair(callback, data)));
}
