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

#include "pointer.h"
#include "xdg-shell-client-protocol.h"

#include <iostream>

#include <linux/input-event-codes.h>
#include <wayland-client.h>

/**
 * @brief Pointer class represents a Wayland pointer device.
 *
 * The Pointer class is responsible for handling Wayland pointer events and managing the cursor.
 */
Pointer::Pointer(struct wl_pointer *pointer, struct wl_shm *shm, struct wl_compositor *compositor, bool enable_cursor) :
        pointer_(pointer),
        shm_(shm),
        enable_cursor_(enable_cursor) {
    wl_pointer_add_listener(pointer, &listener_, this);
    if (enable_cursor_) {
        cursor_ = std::make_unique<Cursor>(this, pointer_, shm_, compositor, enable_cursor_);
        cursor_->enable(0, "basic");
    }
}

/**
 * @class Pointer
 * Represents a WL_POINTER object and handles various pointer events.
 * This class is responsible for releasing and destroying the WL_POINTER object,
 * as well as managing the Cursor object associated with the pointer.
 *
 * @param pointer_ A pointer to the WL_POINTER object.
 * @param shm A pointer to the WL_SHM object.
 * @param compositor A pointer to the WL_COMPOSITOR object.
 * @param enable_cursor A boolean flag indicating whether to enable cursor.
 */
Pointer::~Pointer() {
    if (cursor_)
        cursor_.reset();

    wl_pointer_release(pointer_);
    wl_pointer_destroy(pointer_);
}

/**
 * @class Pointer
 * @brief A class that handles pointer events
 *
 * This class provides functionality to handle various pointer events,
 * such as enter, leave, motion, button, axis, frame, axis source, axis stop,
 * and axis discrete events.
 */
void Pointer::handle_enter(void * /* data */,
                           struct wl_pointer * /* pointer */,
                           uint32_t /* serial */,
                           struct wl_surface * /* surface */,
                           wl_fixed_t /* sx */,
                           wl_fixed_t /* sy */) {
    std::cerr << "Pointer::handle_enter" << std::endl;
}

/**
 * @brief Handles the leave event of the pointer.
 *
 * This function is called when the pointer leaves a surface.
 * It outputs a debug message to the standard error stream.
 *
 * @param data A pointer to user-defined data.
 * @param pointer The pointer object.
 * @param serial The serial number of the event.
 * @param surface The surface that the pointer left.
 */
void Pointer::handle_leave(void * /* data */,
                           struct wl_pointer * /* pointer */,
                           uint32_t /* serial */,
                           struct wl_surface * /* surface */) {
    std::cerr << "Pointer::handle_leave" << std::endl;
}

/**
 * @brief Handles motion events from the pointer device.
 *
 * This function is a callback that is called when a motion
 * event occurs on the pointer device.
 *
 * @param data A pointer to user data.
 * @param pointer The pointer object that triggered the event.
 * @param time The timestamp of the event.
 * @param sx The X coordinate of the pointer's absolute position.
 * @param sy The Y coordinate of the pointer's absolute position.
 */
void Pointer::handle_motion(void * /* data */,
                            struct wl_pointer * /* pointer */,
                            uint32_t /* time */,
                            wl_fixed_t /* sx */,
                            wl_fixed_t /* sy */) {
    std::cerr << "Pointer::handle_motion" << std::endl;
}

/**
 * @brief Determines the resize edge of a component based on the given parameters.
 *
 * @param width The width of the component.
 * @param height The height of the component.
 * @param pointer_x The x-coordinate of the pointer.
 * @param pointer_y The y-coordinate of the pointer.
 * @param margin The margin around the component.
 *
 * @return The resize edge of the component.
 */
enum xdg_toplevel_resize_edge component_edge(const int width, const int height,
                                             const int pointer_x,
                                             const int pointer_y,
                                             const int margin) {
    const bool top = pointer_y < margin;
    const bool bottom = pointer_y > (height - margin);
    const bool left = pointer_x < margin;
    const bool right = pointer_x > (width - margin);

    if (top)
        if (left)
            return XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
        else if (right)
            return XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
        else
            return XDG_TOPLEVEL_RESIZE_EDGE_TOP;
    else if (bottom)
        if (left)
            return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
        else if (right)
            return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
        else
            return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
    else if (left)
        return XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
    else if (right)
        return XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
    else
        return XDG_TOPLEVEL_RESIZE_EDGE_NONE;
}

/**
 * @brief Function to handle button events from the pointer
 *
 * @param data A pointer to user-defined data
 * @param wl_pointer The Wayland pointer object
 * @param serial The serial number of the event
 * @param button The button that triggered the event
 * @param state The state of the button (pressed or released)
 */
void Pointer::handle_button(void * /* data */,
                            struct wl_pointer * /* wl_pointer */,
                            uint32_t /* serial */,
                            uint32_t /* time */,
                            uint32_t button,
                            uint32_t state) {
    std::cerr << "Pointer::handle_button" << std::endl;
    if (button == BTN_LEFT) {
        if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        }
    }
}

/**
 * @brief Handles the axis event of the pointer.
 *
 * This function is called when the pointer generates an axis event, such as a scroll event.
 *
 * @param data      A pointer to user-defined data.
 * @param wl_pointer    A pointer to the wl_pointer object that triggered the event.
 * @param time      The timestamp of the event.
 * @param axis      The axis identifier.
 * @param value     The value of the axis event.
 *
 * @details Prints "Pointer::handle_axis" to the standard error output.
 */
void Pointer::handle_axis(void * /* data */,
                          struct wl_pointer * /* wl_pointer */,
                          uint32_t /* time */,
                          uint32_t /* axis */,
                          wl_fixed_t /* value */) {
    std::cerr << "Pointer::handle_axis" << std::endl;
}

/**
 * @brief Handle a frame event for the pointer.
 *
 * This function is called when a frame event is received for the pointer.
 *
 * @param data The user data associated with the pointer.
 * @param wl_pointer The pointer object.
 */
void Pointer::handle_frame(void * /* data */,
                           struct wl_pointer * /* wl_pointer */) {
    std::cerr << "Pointer::handle_frame" << std::endl;
}

/**
 * @brief Handles the axis source event for the Pointer object.
 *
 * @param data Unused parameter.
 * @param wl_pointer The wl_pointer object associated with the event.
 * @param axis_source The axis source.
 *
 * This function is called when the axis source event is received for the Pointer object.
 * It prints a message to the standard error stream.
 */
void Pointer::handle_axis_source(void * /* data */,
                                 struct wl_pointer * /* wl_pointer */,
                                 uint32_t /* axis_source */) {
    std::cerr << "Pointer::handle_axis_source" << std::endl;
}

/**
 * @brief Handles the stop event for an axis on the pointer.
 *
 * This function is called when an axis stop event is received for the pointer.
 *
 * @param data      A pointer to user-defined data.
 * @param wl_pointer The pointer object that triggered the event.
 * @param time      The timestamp of the event.
 * @param axis      The axis that stopped.
 */
void Pointer::handle_axis_stop(void * /* data */,
                               struct wl_pointer * /* wl_pointer */,
                               uint32_t /* time */,
                               uint32_t /* axis */) {
    std::cerr << "Pointer::handle_axis_stop" << std::endl;
}

/**
 * @brief Handles the discrete axis events for the Pointer.
 *
 * This function is called when a discrete axis event is received.
 *
 * @param data The user data associated with the Pointer.
 * @param wl_pointer The pointer object.
 * @param axis The axis value.
 * @param discrete The discrete value.
 */
void Pointer::handle_axis_discrete(void * /* data */,
                                   struct wl_pointer * /* wl_pointer */,
                                   uint32_t /* axis */,
                                   int32_t /* discrete */) {
    std::cerr << "Pointer::handle_axis_discrete" << std::endl;
}

const struct wl_pointer_listener Pointer::listener_ = {
        .enter = handle_enter,
        .leave = handle_leave,
        .motion = handle_motion,
        .button = handle_button,
        .axis = handle_axis,
        .frame = handle_frame,
        .axis_source = handle_axis_source,
        .axis_stop = handle_axis_stop,
        .axis_discrete = handle_axis_discrete,
};
