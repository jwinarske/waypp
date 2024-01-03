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

#include "cursor.h"

#include <wayland-cursor.h>

constexpr int kCursorSize = 24;
constexpr char kCursorKindBasic[] = "left_ptr";
constexpr char kCursorKindClick[] = "hand";
constexpr char kCursorKindText[] = "left_ptr";
constexpr char kCursorKindForbidden[] = "pirate";

/**
 * @class Cursor
 * @brief Represents a cursor for a Pointer.
 */
Cursor::Cursor(Pointer *parent, struct wl_pointer *pointer, struct wl_shm *shm, struct wl_compositor *compositor,
               bool enable, const char *theme_name)
        : parent_(parent),
          wl_pointer_(pointer),
          wl_shm_(shm),
          wl_surface_(wl_compositor_create_surface(compositor)),
          theme_name_(theme_name),
          enable_(enable) {
    if (enable) {
        theme_ = wl_cursor_theme_load(theme_name_.c_str(), kCursorSize, wl_shm_);
    }
}

/**
 * @brief Destructor for the Cursor class.
 *
 * This function destroys the Cursor object by freeing any allocated resources.
 * It destroys the wl_cursor_theme object pointed to by the theme_ pointer.
 * It also destroys the wl_surface object pointed to by the wl_surface_ pointer.
 */
Cursor::~Cursor() {
    if (theme_)
        wl_cursor_theme_destroy(theme_);

    if (wl_surface_)
        wl_surface_destroy(wl_surface_);
}

/**
 * @brief Sets the cursor for the specified device and kind.
 *
 * @param device The device ID.
 * @param kind The kind of cursor.
 * @return True if the cursor was successfully set, false otherwise.
 *
 * This function sets the cursor for the specified device and kind. If the enable flag is false,
 * the default cursor is set. If the enable flag is true, the cursor is set based on the specified kind.
 *
 * If the specified kind is not found or if an invalid cursor buffer is encountered, the function returns false.
 * If the cursor is successfully set, the function returns true.
 */
bool Cursor::enable(const int32_t device, const char *kind) const {
    (void) device;
    if (!enable_) {
        wl_pointer_set_cursor(wl_pointer_, parent_->get_serial(),
                              wl_surface_, 0, 0);
        wl_surface_damage(wl_surface_, 0, 0, 0, 0);
        wl_surface_commit(wl_surface_);
        return true;
    }

    if (wl_pointer_) {
        const char *cursor_name;
        if (strcmp(kind, "basic") == 0) {
            cursor_name = kCursorKindBasic;
        } else if (strcmp(kind, "click") == 0) {
            cursor_name = kCursorKindClick;
        } else if (strcmp(kind, "text") == 0) {
            cursor_name = kCursorKindText;
        } else if (strcmp(kind, "forbidden") == 0) {
            cursor_name = kCursorKindForbidden;
        } else {
            // Cursor kind
            return false;
        }

        const auto cursor = wl_cursor_theme_get_cursor(theme_, cursor_name);
        if (cursor == nullptr) {
            // not found
            return false;
        }
        const auto cursor_buffer = wl_cursor_image_get_buffer(cursor->images[0]);
        if (cursor_buffer && wl_surface_) {

            wl_pointer_set_cursor(wl_pointer_, parent_->get_serial(),
                                  wl_surface_,
                                  static_cast<int32_t>(cursor->images[0]->hotspot_x),
                                  static_cast<int32_t>(cursor->images[0]->hotspot_y));
            wl_surface_attach(wl_surface_, cursor_buffer, 0, 0);
            wl_surface_damage(wl_surface_, 0, 0,
                              static_cast<int32_t>(cursor->images[0]->width),
                              static_cast<int32_t>(cursor->images[0]->height));
            wl_surface_commit(wl_surface_);
        } else {
            // Failed to set cursor: Invalid Cursor Buffer
            return false;
        }
    }

    return true;
}
