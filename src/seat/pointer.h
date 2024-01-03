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

#ifndef SRC_SEAT_POINTER_H_
#define SRC_SEAT_POINTER_H_

#include <wayland-client.h>

#include "cursor.h"

class Cursor;

class Pointer {
public:
    explicit Pointer(struct wl_pointer *pointer_, struct wl_shm *shm, struct wl_compositor *compositor,
                     bool enable_cursor = true);

    ~Pointer();

    friend class Cursor;

private:
    struct wl_pointer *pointer_;
    struct wl_shm *shm_;

    bool enable_cursor_{};
    std::unique_ptr<Cursor> cursor_;
    uint32_t serial_{};

    [[nodiscard]] uint32_t get_serial() const { return serial_; }

    static void handle_enter(void * /* data */,
                             struct wl_pointer * /* pointer */,
                             uint32_t /* serial */,
                             struct wl_surface * /* surface */,
                             wl_fixed_t /* sx */,
                             wl_fixed_t /* sy */);

    static void handle_leave(void * /* data */,
                             struct wl_pointer * /* pointer */,
                             uint32_t /* serial */,
                             struct wl_surface * /* surface */);

    static void handle_motion(void * /* data */,
                              struct wl_pointer * /* pointer */,
                              uint32_t /* time */,
                              wl_fixed_t /* sx */,
                              wl_fixed_t /* sy */);

    static void handle_button(void * /*  data */,
                              struct wl_pointer * /* wl_pointer */,
                              uint32_t /* serial */,
                              uint32_t /* time */,
                              uint32_t /* button */,
                              uint32_t /* state */);

    static void handle_axis(void * /* data */,
                            struct wl_pointer * /* wl_pointer */,
                            uint32_t /* time */,
                            uint32_t /* axis */,
                            wl_fixed_t /* value */);

    static void handle_frame(void * /* data */,
                             struct wl_pointer * /* wl_pointer */);

    static void handle_axis_source(void * /* data */,
                                   struct wl_pointer * /* wl_pointer */,
                                   uint32_t /* axis_source */);

    static void handle_axis_stop(void * /* data */,
                                 struct wl_pointer * /* wl_pointer */,
                                 uint32_t /* time */,
                                 uint32_t /* axis */);

    static void handle_axis_discrete(void * /* data */,
                                     struct wl_pointer * /* wl_pointer */,
                                     uint32_t /* axis */,
                                     int32_t /* discrete */);

    static const struct wl_pointer_listener listener_;
};

#endif // SRC_SEAT_POINTER_H_