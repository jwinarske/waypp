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

#include "waypp/seat/seat.h"

#include "logging/logging.h"

/**
 * @class Seat
 * @brief Represents a seat in a Wayland compositor.
 *
 * The Seat class provides a representation of a seat in a Wayland compositor.
 * It is used to handle input events from devices such as keyboards, pointers,
 * and touchscreens.
 */
Seat::Seat(wl_seat *seat,
           wl_shm *wl_shm,
           wl_compositor *wl_compositor,
           bool disable_cursor,
           const char *ignore_events)
        : wl_seat_(seat),
          wl_shm_(wl_shm),
          wl_compositor_(wl_compositor),
          disable_cursor_(disable_cursor) {

    wl_seat_add_listener(seat, &listener_, this);

    if (ignore_events) {
        set_event_mask(ignore_events);
    }
}

Seat::~Seat() {
    if (wl_seat_) {
        DLOG_TRACE("[Seat] wl_seat_destroy(wl_seat_)");
        wl_seat_destroy(wl_seat_);
    }
}

/**
 * @class Seat
 * @brief Represents a seat in the Wayland protocol.
 *
 * A seat is a group of input devices used by a user. Each seat is associated
 * with a wl_seat object, which contains multiple capabilities such as pointer,
 * keyboard, and touch.
 */
void Seat::handle_capabilities(void *data,
                               wl_seat *seat,
                               uint32_t caps) {
    const auto obj = static_cast<Seat *>(data);
    if (obj->wl_seat_ != seat) {
        return;
    }

    DLOG_TRACE("Seat::handle_capabilities: {}", caps);

    obj->capabilities_ = caps;

    if (caps & WL_SEAT_CAPABILITY_POINTER && !obj->pointer_) {
        if (!obj->event_mask_.pointer.all) {
            obj->pointer_ = std::make_unique<Pointer>(wl_seat_get_pointer(seat),
                                                      obj->wl_compositor_, obj->wl_shm_,
                                                      obj->disable_cursor_,
                                                      obj->event_mask_.pointer);
        }
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && obj->pointer_) {
        obj->pointer_.reset();
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !obj->keyboard_) {
        if (!obj->event_mask_.keyboard.all) {
            obj->keyboard_ = std::make_unique<Keyboard>(wl_seat_get_keyboard(seat), obj->event_mask_.keyboard);
        }
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && obj->keyboard_) {
        obj->keyboard_.reset();
    }

    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !obj->touch_) {
        if (!obj->event_mask_.touch.all) {
            obj->touch_ = std::make_unique<Touch>(wl_seat_get_touch(seat), obj->event_mask_.touch);
        }
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
void Seat::handle_name(void *data, wl_seat *seat, const char *name) {
    const auto obj = static_cast<Seat *>(data);
    if (obj->wl_seat_ != seat) {
        return;
    }

    DLOG_TRACE("Seat::handle_name: {}", obj->name_);

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

void Seat::event_mask_print() const {
    const std::string out;
    std::stringstream ss(out);
    ss << "Seat Event Mask";
    if (event_mask_.pointer.enabled)
        ss << "\n\tpointer [enabled]";
    if (event_mask_.pointer.all)
        ss << "\n\tpointer";
    if (event_mask_.pointer.axis)
        ss << "\n\tpointer-axis";
    if (event_mask_.pointer.buttons)
        ss << "\n\tpointer-buttons";
    if (event_mask_.pointer.motion)
        ss << "\n\tpointer-motion";
    if (event_mask_.keyboard.enabled)
        ss << "\n\tkeyboard [enabled]";
    if (event_mask_.keyboard.all)
        ss << "\n\tkeyboard";
    if (event_mask_.touch.enabled)
        ss << "\n\ttouch [enabled]";
    if (event_mask_.touch.all)
        ss << "\n\ttouch";

    LOG_INFO(ss.str());
}

void Seat::set_event_mask(const char *ignore_events) {
    std::string ignore_wayland_events(ignore_events);

    std::string events;
    events.reserve(ignore_wayland_events.size());
    for (const char event: ignore_wayland_events) {
        if (event != ' ' && event != '"')
            events += event;
    }

    std::transform(
            events.begin(), events.end(), events.begin(),
            [](const char c) { return std::tolower(static_cast<unsigned char>(c)); });

    std::stringstream ss(events);
    while (ss.good()) {
        std::string event;
        getline(ss, event, ',');
        if (event.rfind("pointer", 0) == 0) {
            event_mask_.pointer.enabled = true;
            if (event == "pointer-axis") {
                event_mask_.pointer.axis = true;
            } else if (event == "pointer-buttons") {
                event_mask_.pointer.buttons = true;
            } else if (event == "pointer-motion") {
                event_mask_.pointer.motion = true;
            } else if (event == "pointer") {
                event_mask_.pointer.all = true;
            }
            if (pointer_) {
                pointer_->set_event_mask(event_mask_.pointer);
            }
        } else if (event.rfind("keyboard", 0) == 0) {
            event_mask_.keyboard.enabled = true;
            if (event == "keyboard") {
                event_mask_.keyboard.all = true;
            }
            if (keyboard_) {
                keyboard_->set_event_mask(event_mask_.keyboard);
            }
        } else if (event.rfind("touch", 0) == 0) {
            event_mask_.touch.all = true;
            if (event == "touch") {
                event_mask_.touch.enabled = true;
            }
            if (touch_) {
                touch_->set_event_mask(event_mask_.touch);
            }
        } else {
            LOG_WARN("Unknown Wayland Event Mask: [{}]", event);
        }
    }

    if (!ignore_wayland_events.empty()) {
        event_mask_print();
    }
}
