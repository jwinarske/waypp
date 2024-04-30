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

#include "logging.h"

/**
 * @class Seat
 * @brief Represents a seat in a Wayland compositor.
 *
 * The Seat class provides a representation of a seat in a Wayland compositor. It is used to handle input events from
 * devices such as keyboards, pointers, and touchscreens.
 */
Seat::Seat(struct wl_seat *seat, const std::optional<struct wl_shm *> &wl_shm, struct wl_compositor *wl_compositor,
           bool disable_cursor)
        : wl_seat_(seat), wl_shm_(wl_shm), wl_compositor_(wl_compositor), disable_cursor_(disable_cursor) {
    wl_seat_add_listener(seat, &listener_, this);
}

Seat::~Seat() {
    if(wl_seat_) {
        wl_seat_destroy(wl_seat_);
    }
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
    if (obj->wl_seat_ != seat) {
        return;
    }

    SPDLOG_TRACE("Seat::handle_capabilities: {}", caps);

    obj->capabilities_ = caps;

    if (caps & WL_SEAT_CAPABILITY_POINTER && !obj->pointer_) {
        obj->pointer_ = std::make_unique<Pointer>(wl_seat_get_pointer(seat), obj->wl_compositor_, obj->wl_shm_,
                                                  obj->disable_cursor_);
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

    for (auto observer: obj->observers_) {
        observer->notify_seat_capabilities(obj, seat, caps);
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
    if (obj->wl_seat_ != seat) {
        return;
    }

    SPDLOG_TRACE("Seat::handle_name: {}", obj->name_);

    obj->name_ = name;

    for (auto observer: obj->observers_) {
        observer->notify_seat_name(obj, seat, name);
    }
}

std::optional<Keyboard *> Seat::get_keyboard() const {
    if (keyboard_) {
        return keyboard_.get();
    }
    return {};
}

std::optional<Pointer *> Seat::get_pointer() const {
    if (pointer_) {
        return pointer_.get();
    }
    return {};
}
