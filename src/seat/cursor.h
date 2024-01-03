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

#ifndef SRC_SEAT_CURSOR_H_
#define SRC_SEAT_CURSOR_H_

#include <cstdint>

#include <wayland-client.h>

#include "window_manager/display.h"
#include "pointer.h"

class Display;

class Pointer;

class Cursor {
public:
    Cursor(Pointer *parent, struct wl_pointer *pointer, struct wl_shm *shm, struct wl_compositor *compositor,
           bool enable,
           const char *theme_name = "DMZ-White");

    ~Cursor();

    bool enable(int32_t device, const char *kind) const;

private:
    Pointer *parent_;
    struct wl_pointer *wl_pointer_;
    struct wl_surface *wl_surface_;
    struct wl_cursor_theme *theme_;
    struct wl_shm *wl_shm_;
    bool enable_;

    std::string theme_name_;
};

#endif // SRC_SEAT_CURSOR_H_