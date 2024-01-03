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

#ifndef SRC_SEAT_SEAT_H_
#define SRC_SEAT_SEAT_H_

#include <memory>
#include <string>

#include <wayland-client.h>

#include "keyboard.h"
#include "pointer.h"
#include "touch.h"

class Keyboard;

class Pointer;

class Touch;

class Seat {
public:
    explicit Seat(struct wl_seat *seat, struct wl_shm *shm, struct wl_compositor *compositor, bool enable_cursor,
                  uint32_t version);

    [[nodiscard]] struct wl_seat *get_seat() const { return wl_seat_; };

    [[nodiscard]] uint32_t get_capabilities() const { return capabilities_; };

    [[nodiscard]] const std::string &get_name() const { return name_; };

private:
    struct wl_seat *wl_seat_;
    struct wl_shm *wl_shm_;
    struct wl_compositor *wl_compositor_;
    bool enable_cursor_;
    uint32_t version_;
    uint32_t capabilities_;
    std::string name_;

    std::unique_ptr<Keyboard> keyboard_;
    std::unique_ptr<Pointer> pointer_;
    std::unique_ptr<Touch> touch_;

    static void handle_capabilities(void * /* data */,
                                    struct wl_seat * /* seat */,
                                    uint32_t /* caps */);

    static void handle_name(void * /* data */,
                            struct wl_seat * /* seat */,
                            const char * /* name */);

    static const struct wl_seat_listener listener_;
};

#endif // SRC_SEAT_SEAT_H_