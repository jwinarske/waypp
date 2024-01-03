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

#ifndef SRC_SEAT_TOUCH_H_
#define SRC_SEAT_TOUCH_H_

#include <cstdint>

#include <wayland-client.h>

class Touch {
public:
    explicit Touch(struct wl_touch *touch);

    ~Touch();

private:
    struct wl_touch *touch_;

    static void handle_down(void *data,
                            struct wl_touch * /* wl_touch */,
                            uint32_t /* serial */,
                            uint32_t /* time */,
                            struct wl_surface *surface,
                            int32_t id,
                            wl_fixed_t x_w,
                            wl_fixed_t y_w);

    static void handle_up(void *data,
                          struct wl_touch * /* wl_touch */,
                          uint32_t /* serial */,
                          uint32_t /* time */,
                          int32_t id);

    static void handle_motion(void *data,
                              struct wl_touch * /* wl_touch */,
                              uint32_t /* time */,
                              int32_t id,
                              wl_fixed_t x_w,
                              wl_fixed_t y_w);

    static void handle_cancel(void *data, struct wl_touch * /* wl_touch */);

    static void handle_frame(void * /* data */,
                             struct wl_touch * /* wl_touch */);

    static const struct wl_touch_listener listener_;
};

#endif // SRC_SEAT_TOUCH_H_