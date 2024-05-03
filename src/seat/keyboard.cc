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

// workaround for Wayland macro not compiling in C++
#define WL_ARRAY_FOR_EACH(pos, array, type)                             \
  for (pos = (type)(array)->data;                                       \
       (const char*)pos < ((const char*)(array)->data + (array)->size); \
       (pos)++)

/**
 * @class Keyboard
 * @brief Represents a keyboard device
 *
 * The Keyboard class provides a wrapper for a keyboard device,
 * which interacts with the Wayland compositor.
 */
Keyboard::Keyboard(struct wl_keyboard *keyboard) : wl_keyboard_(keyboard),
                                                   xkb_context_(xkb_context_new(
                                                           XKB_CONTEXT_NO_FLAGS)) {
    SPDLOG_DEBUG("Keyboard");
    wl_keyboard_add_listener(wl_keyboard_, &keyboard_listener_, this);
}

/**
 * @class Keyboard
 * @brief Represents a keyboard input device.
 *
 * The Keyboard class manages the interaction with a Wayland keyboard input device.
 */
Keyboard::~Keyboard() {
    if (repeat_.timer) {
        struct itimerspec its{};
        timer_settime(repeat_.timer, 0, &its, nullptr);
        timer_delete(repeat_.timer);
    }

    wl_keyboard_release(wl_keyboard_);

    if (xkb_state_) {
        xkb_state_unref(xkb_state_);
    }
    if (xkb_keymap_) {
        xkb_keymap_unref(xkb_keymap_);
    }
    if (xkb_context_) {
        xkb_context_unref(xkb_context_);
    }
}

void Keyboard::handle_keymap(void *data,
                             struct wl_keyboard *wl_keyboard,
                             uint32_t format,
                             int fd,
                             uint32_t size) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->wl_keyboard_ != wl_keyboard) {
        return;
    }

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        spdlog::critical(
                "Usage with a libxkbcommon is currently required.  Please file a bug with configuration information to enable support.");
        abort();
    }

    char *keymap_string;
    /// From version 7 onwards, the fd must be mapped with MAP_PRIVATE by the recipient, as MAP_SHARED may fail.
    if (wl_keyboard_get_version(wl_keyboard) >= 7) {
        keymap_string = static_cast<char *>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    } else {
        keymap_string = static_cast<char *>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
    }
    xkb_keymap_unref(obj->xkb_keymap_);
    obj->xkb_keymap_ = xkb_keymap_new_from_string(obj->xkb_context_, keymap_string,
                                                  XKB_KEYMAP_FORMAT_TEXT_V1,
                                                  XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(keymap_string, size);
    close(fd);
    xkb_state_unref(obj->xkb_state_);
    obj->xkb_state_ = xkb_state_new(obj->xkb_keymap_);

    for (auto observer: obj->observers_) {
        observer->notify_keyboard_keymap(obj, wl_keyboard, format, fd, size);
    }
}

void Keyboard::handle_enter(void *data,
                            struct wl_keyboard *wl_keyboard,
                            uint32_t serial,
                            struct wl_surface *wl_surface,
                            struct wl_array *keys) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->wl_keyboard_ != wl_keyboard) {
        return;
    }

    SPDLOG_TRACE("[Keyboard] handle_enter");

    obj->wl_surface = wl_surface;

    if (keys->size) {
        const uint32_t *key;
        WL_ARRAY_FOR_EACH(key, keys, const uint32_t*) {
            handle_key(data, wl_keyboard, serial, 0, *key, WL_KEYBOARD_KEY_STATE_PRESSED);
        }
    }

    for (auto observer: obj->observers_) {
        observer->notify_keyboard_enter(obj, wl_keyboard, serial, wl_surface, keys);
    }
}

void Keyboard::handle_leave(void *data,
                            struct wl_keyboard *wl_keyboard,
                            uint32_t serial,
                            struct wl_surface *wl_surface) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->wl_keyboard_ != wl_keyboard) {
        return;
    }
    SPDLOG_TRACE("[Keyboard] handle_leave");

    obj->wl_surface = nullptr;

    for (auto observer: obj->observers_) {
        observer->notify_keyboard_leave(obj, wl_keyboard, serial, wl_surface);
    }
}

void Keyboard::handle_key(void *data,
                          struct wl_keyboard *wl_keyboard,
                          uint32_t serial,
                          uint32_t time,
                          uint32_t key,
                          uint32_t state) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->wl_keyboard_ != wl_keyboard) {
        return;
    }

    if (!obj->xkb_state_)
        return;

    /// translate scancode to XKB scancode
    auto xkb_scancode = key + 8;
    auto key_repeats = xkb_keymap_key_repeats(obj->xkb_keymap_, xkb_scancode);

    const xkb_keysym_t *key_syms;
    auto xdg_keysym_count = xkb_state_key_get_syms(obj->xkb_state_, xkb_scancode, &key_syms);

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (key_repeats) {
            // start/restart timer
            struct itimerspec in{};
            in.it_value.tv_nsec = obj->repeat_.delay * 1000000;
            in.it_interval.tv_nsec = obj->repeat_.rate * 1000000;
            timer_settime(obj->repeat_.timer, 0, &in, nullptr);

            // update notify values
            obj->repeat_.notify = {
                    .serial = serial,
                    .time = time,
                    .xkb_scancode = xkb_scancode,
                    .key_repeats = key_repeats,
                    .xdg_keysym_count = xdg_keysym_count,
                    .key_syms = key_syms,
            };
        }

    } else if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (obj->repeat_.notify.xkb_scancode == xkb_scancode) {
            // stop timer
            struct itimerspec its{};
            timer_settime(obj->repeat_.timer, 0, &its, nullptr);
        }
    }

    for (auto observer: obj->observers_) {
        observer->notify_keyboard_key(obj, wl_keyboard, serial, time, xkb_scancode, key_repeats, state,
                                      xdg_keysym_count, key_syms);
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
    if (obj->wl_keyboard_ != keyboard) {
        return;
    }

    SPDLOG_TRACE("[Keyboard] handle_modifiers");

    xkb_state_update_mask(obj->xkb_state_, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

void Keyboard::handle_repeat_info(void *data,
                                  struct wl_keyboard *keyboard,
                                  int32_t rate,
                                  int32_t delay) {
    const auto obj = static_cast<Keyboard *>(data);
    if (obj->wl_keyboard_ != keyboard) {
        return;
    }

    SPDLOG_TRACE("[Keyboard] handle_repeat_info: rate: {}, delay: {}", rate, delay);

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

    for (auto observer: obj->observers_) {
        observer->notify_keyboard_key(obj, obj->repeat_.notify.wl_keyboard, obj->repeat_.notify.serial,
                                      obj->repeat_.notify.time, obj->repeat_.notify.xkb_scancode,
                                      obj->repeat_.notify.key_repeats, WL_KEYBOARD_KEY_STATE_PRESSED,
                                      obj->repeat_.notify.xdg_keysym_count, obj->repeat_.notify.key_syms);
    }
}
