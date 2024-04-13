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

#include <wayland-client.h>

class Pointer {
public:
    explicit Pointer(struct wl_pointer *pointer);

    ~Pointer();

private:
    struct wl_pointer *pointer_;
    uint32_t serial_{};

    static void handle_enter(void *data,
                             struct wl_pointer *pointer,
                             uint32_t serial,
                             struct wl_surface *surface,
                             wl_fixed_t sx,
                             wl_fixed_t sy);

    static void handle_leave(void *data,
                             struct wl_pointer *pointer,
                             uint32_t serial,
                             struct wl_surface *surface);

    static void handle_motion(void *data,
                              struct wl_pointer *pointer,
                              uint32_t time,
                              wl_fixed_t sx,
                              wl_fixed_t sy);

    static void handle_button(void *data,
                              struct wl_pointer *wl_pointer,
                              uint32_t serial,
                              uint32_t time,
                              uint32_t button,
                              uint32_t state);

    static void handle_axis(void *data,
                            struct wl_pointer *wl_pointer,
                            uint32_t time,
                            uint32_t axis,
                            wl_fixed_t value);

    static void handle_frame(void *data,
                             struct wl_pointer *wl_pointer);

    static void handle_axis_source(void *data,
                                   struct wl_pointer *wl_pointer,
                                   uint32_t axis_source);

    static void handle_axis_stop(void *data,
                                 struct wl_pointer *wl_pointer,
                                 uint32_t time,
                                 uint32_t axis);

    static void handle_axis_discrete(void *data,
                                     struct wl_pointer *wl_pointer,
                                     uint32_t axis,
                                     int32_t discrete);

    static constexpr struct wl_pointer_listener pointer_listener_ = {
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
};
