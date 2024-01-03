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

#include "seat.h"

#include <cassert>

/**
 * @class Seat
 * @brief Represents a seat in a Wayland compositor.
 *
 * The Seat class provides a representation of a seat in a Wayland compositor. It is used to handle input events from
 * devices such as keyboards, pointers, and touchscreens.
 */
Seat::Seat(struct wl_seat *seat, struct wl_shm *shm, struct wl_compositor *compositor, bool enable_cursor,
           uint32_t version) :
        wl_seat_(seat),
        wl_shm_(shm),
        wl_compositor_(compositor),
        enable_cursor_(enable_cursor),
        version_(version),
        capabilities_() {
    wl_seat_add_listener(seat, &listener_, this);
}

/**
 * @class Seat
 * @brief Represents a seat in the Wayland protocol.
 *
 * A seat is a group of input devices used by a user. Each seat is associated with a wl_seat object,
 * which contains multiple capabilities such as pointer, keyboard, and touch.
 */
void Seat::handle_capabilities(void *data,
                               struct wl_seat *seat,
                               uint32_t caps) {
    const auto obj = static_cast<Seat *>(data);
    assert(obj->wl_seat_ == seat);
    obj->capabilities_ = caps;

    if (caps & WL_SEAT_CAPABILITY_POINTER && !obj->pointer_) {
        obj->pointer_ = std::make_unique<Pointer>(wl_seat_get_pointer(seat), obj->wl_shm_, obj->wl_compositor_,
                                                  obj->enable_cursor_);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && obj->pointer_) {
        obj->pointer_.reset();
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !obj->keyboard_) {
        obj->keyboard_ = std::make_unique<Keyboard>(wl_seat_get_keyboard(seat));
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && obj->keyboard_) {
        obj->keyboard_.reset();
    }

    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !obj->touch_) {
        obj->touch_ = std::make_unique<Touch>(wl_seat_get_touch(seat));
    } else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && obj->touch_) {
        obj->touch_.reset();
    }
}

/**
 * @brief Handles the name event of the Seat object.
 *
 * This function is called when the name of the seat is received from
 * the Wayland server. It updates the name_ member variable of the Seat
 * object with the provided name.
 *
 * @param data A pointer to the Seat object.
 * @param seat The wl_seat object for which the event occurred.
 * @param name The name of the seat.
 */
void Seat::handle_name(void *data,
                       struct wl_seat *seat,
                       const char *name) {
    const auto obj = static_cast<Seat *>(data);
    assert(obj->wl_seat_ == seat);
    obj->name_ = name;
}

const struct wl_seat_listener Seat::listener_ = {
        .capabilities = handle_capabilities,
        .name = handle_name,
};
