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

#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "logging.h"

/**
 * @class Keyboard
 * @brief Represents a keyboard device
 *
 * The Keyboard class provides a wrapper for a keyboard device,
 * which interacts with the Wayland compositor.
 */
Keyboard::Keyboard(struct wl_keyboard *keyboard) : keyboard_(keyboard),
                                                   xkb_context_(xkb_context_new(
                                                           XKB_CONTEXT_NO_FLAGS)) {
    SPDLOG_DEBUG("Keyboard");
    wl_keyboard_add_listener(keyboard_, &keyboard_listener_, this);
}

/**
 * @class Keyboard
 * @brief Represents a keyboard input device.
 *
 * The Keyboard class manages the interaction with a Wayland keyboard input device.
 */
Keyboard::~Keyboard() {
    if (repeat_.timer) {
        timer_delete(repeat_.timer);
    }
    wl_keyboard_release(keyboard_);
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

    for (auto observer: obj->observers_) {
        observer->notify_key(
                obj, state == WL_KEYBOARD_KEY_STATE_RELEASED, keysym, xkb_scancode, 0
        );
    }

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (xkb_keymap_key_repeats(obj->keymap_, xkb_scancode)) {
            obj->keysym_pressed_ = keysym;
            obj->start_repeat(xkb_scancode);
        } else {
            SPDLOG_DEBUG("key does not repeat: 0x{:x}", xkb_scancode);
        }

    } else if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (obj->repeat_.code == xkb_scancode) {
            obj->stop_repeat();
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

    obj->repeat_.rate = rate;
    obj->repeat_.delay = delay;

    if (!obj->repeat_.timer) {

        /// Setup signal event
        obj->repeat_.sev.sigev_notify = SIGEV_SIGNAL;
        obj->repeat_.sev.sigev_signo = SIGRTMIN;
        obj->repeat_.sev.sigev_value.sival_ptr = data;
        auto res = timer_create(CLOCK_REALTIME, &obj->repeat_.sev, &obj->repeat_.timer);
        if (res != 0) {
            spdlog::critical("Error timer_create: {}", strerror(errno));
            abort();
        }

        /// Setup signal action
        obj->repeat_.sa.sa_flags = SA_SIGINFO;
        obj->repeat_.sa.sa_sigaction = repeat_callback;
        sigemptyset(&obj->repeat_.sa.sa_mask);
        if (sigaction(SIGRTMIN, &obj->repeat_.sa, nullptr) == -1) {
            spdlog::critical("Error sigaction: {}", strerror(errno));
            abort();
        }
    }
}

const struct wl_keyboard_listener Keyboard::keyboard_listener_ = {
        .keymap = handle_keymap,
        .enter = handle_enter,
        .leave = handle_leave,
        .key = handle_key,
        .modifiers = handle_modifiers,
        .repeat_info = handle_repeat_info,
};

void Keyboard::repeat_callback(int /* sig */, siginfo_t *si, void * /* uc */) {
    auto obj = static_cast<Keyboard *>(si->_sifields._rt.si_sigval.sival_ptr);
    if (obj->repeat_.code != XKB_KEY_NoSymbol) {
        for (auto observer: obj->observers_) {
            observer->notify_key(obj, false, obj->keysym_pressed_, obj->repeat_.code, 0);
        }
    }
}

void Keyboard::start_repeat(uint32_t repeat_code) {
    repeat_.code = repeat_code;

    SPDLOG_DEBUG("Keyboard::start_repeat: {}", repeat_code);

    struct itimerspec in{};
    in.it_value.tv_nsec = repeat_.delay * 1000000;
    in.it_interval.tv_nsec = repeat_.rate * 1000000;
    auto res = timer_settime(repeat_.timer, 0, &in, nullptr);
    if (res != 0) {
        spdlog::critical("Error timer_settime: {}", strerror(errno));
        abort();
    }
}

void Keyboard::stop_repeat() {
    repeat_.code = XKB_KEY_NoSymbol;

    SPDLOG_DEBUG("Keyboard::stop_repeat");

    /// Stop timer
    if (repeat_.timer) {
        struct itimerspec its{};
        timer_settime(repeat_.timer, 0, &its, nullptr);
    }
}
