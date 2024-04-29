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

#include <memory>
#include <optional>
#include <string>

#include <wayland-client.h>

#include "keyboard.h"
#include "pointer.h"
#include "touch.h"

class Keyboard;

class Pointer;

class Touch;

class SeatObserver {
public:
    virtual ~SeatObserver() = default;

    virtual void notify_seat_name(void *data,
                                  struct wl_seat *seat,
                                  const char *name) = 0;

    virtual void notify_seat_capabilities(void *data,
                                          struct wl_seat *seat,
                                          uint32_t caps) = 0;
};

class Seat {
public:
    explicit Seat(struct wl_seat *seat);

    void register_observer(SeatObserver *observer) {
        observers_.push_back(observer);
    }

    void unregister_observer(SeatObserver *observer) {
        observers_.remove(observer);
    }

    [[nodiscard]] struct wl_seat *get_seat() const { return wl_seat_; }

    [[nodiscard]] uint32_t get_capabilities() const { return capabilities_; }

    [[nodiscard]] const std::string &get_name() const { return name_; }

    [[nodiscard]] std::optional<Keyboard *> get_keyboard() const;

    [[nodiscard]] std::optional<Pointer *> get_pointer() const;

    bool is_ready() const { return ready_; }

private:
    struct wl_seat *wl_seat_;
    uint32_t capabilities_;
    std::string name_;
    bool ready_{};

    std::list<SeatObserver *> observers_;


    std::unique_ptr<Keyboard> keyboard_;
    std::unique_ptr<Pointer> pointer_;
    std::unique_ptr<Touch> touch_;

    static void handle_capabilities(void *data,
                                    struct wl_seat *seat,
                                    uint32_t caps);

    static void handle_name(void *data,
                            struct wl_seat *seat,
                            const char *name);

    static constexpr struct wl_seat_listener listener_ = {
            .capabilities = handle_capabilities,
            .name = handle_name,
    };
};
