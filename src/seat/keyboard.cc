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

#include "keyboard.h"

#include <memory>

#include <wayland-client.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

#include "timer.h"
#include "logging.h"

/**
 * @class Keyboard
 * @brief Represents a keyboard device
 *
 * The Keyboard class provides a wrapper for a keyboard device,
 * which interacts with the Wayland compositor.
 */
Keyboard::Keyboard(struct wl_keyboard *keyboard, KeyCallback key_callback) : keyboard_(keyboard),
                                                                             xkb_context_(xkb_context_new(
                                                                                     XKB_CONTEXT_NO_FLAGS)),
                                                                             key_callback_(key_callback),
                                                                             repeat_timer_(
                                                                                     std::make_unique<EventTimer>(
                                                                                             CLOCK_MONOTONIC,
                                                                                             repeat_callback, this)) {
    SPDLOG_DEBUG("Keyboard");
    repeat_timer_->set_timerspec(40, 400);
    wl_keyboard_add_listener(keyboard_, &keyboard_listener_, this);
}

/**
 * @class Keyboard
 * @brief Represents a keyboard input device.
 *
 * The Keyboard class manages the interaction with a Wayland keyboard input device.
 */
Keyboard::~Keyboard() {
    repeat_timer_.reset();
    wl_keyboard_release(keyboard_);
}

void Keyboard::repeat_callback(void *data) {
    auto obj = static_cast<Keyboard *>(data);
    if (XKB_KEY_NoSymbol != obj->repeat_code_) {
        if (obj->key_callback_) {
            obj->key_callback_(data, false, obj->keysym_pressed_, obj->repeat_code_, 0);
        }
    }
}

gboolean Keyboard::handle_repeat(Keyboard *keyboard) {

    if (keyboard) {
        if (keyboard->key_repeat_rate_) {
            keyboard->key_timeout_id_ = g_timeout_add(static_cast<guint>(keyboard->key_repeat_rate_),
                                                      reinterpret_cast<GSourceFunc>(handle_repeat), keyboard);
            return TRUE;
        } else {
            g_source_remove(keyboard->key_timeout_id_);
            return FALSE;
        }
    }
    return TRUE;
}

void Keyboard::handle_keymap(void *data,
                             struct wl_keyboard *keyboard,
                             uint32_t /* format */,
                             int fd,
                             uint32_t size) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->keyboard_ != keyboard) {
        return;
    }
    char *keymap_string = static_cast<char *>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
    xkb_keymap_unref(obj->keymap_);
    obj->keymap_ = xkb_keymap_new_from_string(obj->xkb_context_, keymap_string,
                                              XKB_KEYMAP_FORMAT_TEXT_V1,
                                              XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(keymap_string, size);
    close(fd);
    xkb_state_unref(obj->xkb_state_);
    obj->xkb_state_ = xkb_state_new(obj->keymap_);
}

void Keyboard::handle_enter(void *data,
                            struct wl_keyboard *keyboard,
                            uint32_t /* serial */,
                            struct wl_surface *surface,
                            struct wl_array * /* keys */) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->keyboard_ != keyboard) {
        return;
    }
    SPDLOG_DEBUG("[Keyboard] handle_enter");
    obj->active_surface_ = surface;
}

void Keyboard::handle_leave(void *data,
                            struct wl_keyboard *keyboard,
                            uint32_t /* serial */,
                            struct wl_surface * /* surface */) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->keyboard_ != keyboard) {
        return;
    }
    SPDLOG_DEBUG("[Keyboard] handle_leave");
    obj->active_surface_ = nullptr;
}

void Keyboard::handle_key(void *data,
                          struct wl_keyboard *keyboard,
                          uint32_t /* serial */,
                          uint32_t /* time */,
                          uint32_t key,
                          uint32_t state) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->keyboard_ != keyboard) {
        return;
    }

    if (!obj->xkb_state_)
        return;

    // translate scancode to XKB scancode
    const uint32_t xkb_scancode = key + 8;

    // Gets the single keysym obtained from pressing a particular key in a given
    // keyboard state.
    xkb_keysym_t keysym = xkb_state_key_get_one_sym(obj->xkb_state_, xkb_scancode);
    if (keysym == XKB_KEY_NoSymbol) {
        const xkb_keysym_t *key_symbols;
        const int res =
                xkb_state_key_get_syms(obj->xkb_state_, xkb_scancode, &key_symbols);
        if (res == 0) {
            keysym = XKB_KEY_NoSymbol;
        } else {
            // only use the first symbol until the use case for two is clarified
            keysym = key_symbols[0];
            for (int i = 0; i < res; i++) {
                SPDLOG_DEBUG("xkb keysym: 0x{}", key_symbols[i]);
            }
        }
    }

    if (obj->key_callback_) {
        obj->key_callback_(obj, state == WL_KEYBOARD_KEY_STATE_RELEASED, keysym, xkb_scancode, 0);
    }

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (xkb_keymap_key_repeats(obj->keymap_, xkb_scancode)) {
            SPDLOG_DEBUG("xkb_keymap_key_repeats: {}", xkb_scancode);
            obj->keysym_pressed_ = keysym;
            Keyboard::set_repeat_code(obj, xkb_scancode);
            obj->repeat_timer_->arm();
        } else {
            SPDLOG_DEBUG("key does not repeat: 0x{:x}", xkb_scancode);
        }

    } else if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (obj->repeat_code_ == xkb_scancode) {
            obj->repeat_timer_->disarm();
            Keyboard::set_repeat_code(obj, XKB_KEY_NoSymbol);
        }
    }
}

void Keyboard::handle_modifiers(void *data,
                                struct wl_keyboard *keyboard,
                                uint32_t /* serial */,
                                uint32_t mods_depressed,
                                uint32_t mods_latched,
                                uint32_t mods_locked,
                                uint32_t group) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->keyboard_ != keyboard) {
        return;
    }
    SPDLOG_DEBUG("[Keyboard] handle_modifiers");
    xkb_state_update_mask(obj->xkb_state_, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

void Keyboard::handle_repeat_info(void *data,
                                  struct wl_keyboard *keyboard,
                                  int32_t rate,
                                  int32_t delay) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->keyboard_ != keyboard) {
        return;
    }
    SPDLOG_DEBUG("[Keyboard] handle_repeat_info: rate: {}, delay: {}", rate, delay);
    obj->key_timeout_id_ = g_timeout_add(static_cast<guint>(delay), reinterpret_cast<GSourceFunc>(handle_repeat), obj);
    obj->key_repeat_rate_ = rate;
    obj->key_timeout_id_ = g_timeout_add(static_cast<guint>(delay),
                                         reinterpret_cast<GSourceFunc>(handle_repeat), obj);
}

const struct wl_keyboard_listener Keyboard::keyboard_listener_ = {
        .keymap = handle_keymap,
        .enter = handle_enter,
        .leave = handle_leave,
        .key = handle_key,
        .modifiers = handle_modifiers,
        .repeat_info = handle_repeat_info,
};
