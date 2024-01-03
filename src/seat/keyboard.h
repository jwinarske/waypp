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

#ifndef SRC_SEAT_KEYBOARD_H_
#define SRC_SEAT_KEYBOARD_H_

#include <cstdint>

#include <glib-2.0/glib.h>
#include <xkbcommon/xkbcommon.h>

class Keyboard {
public:
    explicit Keyboard(struct wl_keyboard *keyboard);

    ~Keyboard();

private:
    struct wl_keyboard *keyboard_;
    struct wl_surface *active_surface_{};
    struct xkb_context *xkb_context_;
    struct xkb_keymap *keymap_{};
    struct xkb_state *xkb_state_{};

    xkb_keysym_t keysym_pressed_{};
    guint key_timeout_id_{};

    int32_t key_repeat_rate_{};

    static gboolean handle_repeat(Keyboard *keyboard);

    static void handle_enter(void * /* data */,
                             struct wl_keyboard * /* keyboard */,
                             uint32_t /* serial */,
                             struct wl_surface * /* surface */,
                             struct wl_array * /* keys */);

    static void handle_leave(void * /* data */,
                             struct wl_keyboard * /* keyboard */,
                             uint32_t /* serial */,
                             struct wl_surface * /* surface */);

    static void handle_keymap(void * /* data */,
                              struct wl_keyboard * /* keyboard */,
                              uint32_t /* format */,
                              int /* fd */,
                              uint32_t /* size */);

    static void handle_key(void *  /* data */,
                           struct wl_keyboard * /* keyboard */,
                           uint32_t /* serial */,
                           uint32_t /* time */,
                           uint32_t /* key */,
                           uint32_t /* state */);

    static void handle_modifiers(void * /* data */,
                                 struct wl_keyboard * /* keyboard */,
                                 uint32_t /* serial */,
                                 uint32_t /* mods_depressed */,
                                 uint32_t /* mods_latched */,
                                 uint32_t /* mods_locked */,
                                 uint32_t /* group */);

    static void handle_repeat_info(void * /* data */,
                                   struct wl_keyboard * /* wl_keyboard */,
                                   int32_t /* rate */,
                                   int32_t /* delay */);

    static const struct wl_keyboard_listener listener_;
};

#endif // SRC_SEAT_KEYBOARD_H_