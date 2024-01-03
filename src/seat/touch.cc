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

#include "touch.h"

#include <iostream>

/**
 * @class Touch
 *
 * The Touch class represents a touch input device.
 */
Touch::Touch(struct wl_touch *touch) : touch_(touch) {
    wl_touch_add_listener(touch, &listener_, this);
}

/**
 * @brief Destructor for the Touch class.
 *
 * This destructor releases and destroys the `wl_touch` object associated with the Touch instance.
 */
Touch::~Touch() {
    wl_touch_release(touch_);
    wl_touch_destroy(touch_);
}

/**
 * @brief Handles the touch down event.
 *
 * This function is called when a touch down event occurs. It outputs a debug message to the standard error stream.
 *
 * @param data Pointer to the data associated with the touch object.
 * @param wl_touch Pointer to the wl_touch object.
 * @param serial The serial number of the event.
 * @param time The time stamp of the event.
 * @param surface Pointer to the wl_surface object that received the touch event.
 * @param id The ID of the touch point.
 * @param x_w The X coordinate of the touch point in wl_fixed_t format.
 * @param y_w The Y coordinate of the touch point in wl_fixed_t format.
 */
void Touch::handle_down(void * /* data */,
                        struct wl_touch * /* wl_touch */,
                        uint32_t /* serial */,
                        uint32_t /* time */,
                        struct wl_surface * /* surface */,
                        int32_t /* id */,
                        wl_fixed_t /* x_w */,
                        wl_fixed_t /* y_w */) {
    std::cerr << "Touch::handle_down" << std::endl;
}

/**
 * @brief Handles the touch up event.
 *
 * This function is a callback that is triggered when the touch point is lifted off the surface.
 *
 * @param data A pointer to the user-specified data.
 * @param wl_touch A pointer to the wl_touch object.
 * @param serial The serial number of the event.
 * @param time The timestamp of the event.
 * @param id The unique identifier of the touch point.
 *
 * @return None.
 */
void Touch::handle_up(void * /* data */,
                      struct wl_touch * /* wl_touch */,
                      uint32_t /* serial */,
                      uint32_t /* time */,
                      int32_t /* id */) {
    std::cerr << "Touch::handle_up" << std::endl;
}

/**
 * @brief Handles motion events from a touch device.
 *
 * This function is called when there is a motion event from a touch device.
 * It prints a message indicating that the motion event is being handled.
 *
 * @param data  A pointer to user-defined data.
 * @param wl_touch  A pointer to the wl_touch object associated with the motion event.
 * @param time  The timestamp of the motion event.
 * @param id  The ID of the touch point.
 * @param x_w  The x coordinate of the touch point, in wl_fixed_t format.
 * @param y_w  The y coordinate of the touch point, in wl_fixed_t format.
 *
 * @return None.
 */
void Touch::handle_motion(void * /* data */,
                          struct wl_touch * /* wl_touch */,
                          uint32_t /* time */,
                          int32_t /* id */,
                          wl_fixed_t /* x_w */,
                          wl_fixed_t /* y_w */) {
    std::cerr << "Touch::handle_motion" << std::endl;
}

/**
 * @brief Handles cancel events from the wl_touch interface.
 *
 * This function is called when a cancel event is received from the wl_touch interface.
 * It prints a debug message indicating that the cancel event has been handled.
 *
 * @param data Pointer to custom data associated with the touch interface
 * @param wl_touch Unused parameter - Touch object associated with the event
 *
 * @return void
 */
void Touch::handle_cancel(void * /* data */, struct wl_touch * /* wl_touch */) {
    std::cerr << "Touch::handle_cancel" << std::endl;
}

/**
 * @class Touch
 * @brief The Touch class represents a touch input device.
 *
 * It handles touch events from a wl_touch object and provides callback functions for various touch events.
 */
void Touch::handle_frame(void * /* data */,
                         struct wl_touch * /* wl_touch */) {
    std::cerr << "Touch::handle_frame" << std::endl;
}

const struct wl_touch_listener Touch::listener_ = {
        .down = handle_down,
        .up = handle_up,
        .motion = handle_motion,
        .frame = handle_frame,
        .cancel = handle_cancel,
};