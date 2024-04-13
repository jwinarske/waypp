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

#include <wayland-cursor.h>

class Cursor {
public:
    explicit Cursor(struct wl_shm *shm, struct wl_compositor *compositor, int size = 24);

    ~Cursor();

    void update_pointer(struct wl_pointer *pointer, uint32_t serial, const char *name = "left_ptr");

    // Disallow copy and assign.
    Cursor(const Cursor &) = delete;

    Cursor &operator=(const Cursor &) = delete;

private:
    struct wl_surface *wl_surface_;
    struct wl_cursor_theme *theme_;
};
