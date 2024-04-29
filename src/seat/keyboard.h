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

#include <cstdint>
#include <list>
#include <memory>
#include <mutex>

#include <glib-2.0/glib.h>
#include <xkbcommon/xkbcommon.h>
#include "timer.h"

class KeyboardObserver {
public:
    virtual ~KeyboardObserver() = default;

    virtual void notify_key(void *data,
                        bool released,
                        xkb_keysym_t keysym,
                        uint32_t xkb_scancode,
                        uint32_t modifiers) = 0;
};

class Keyboard {
public:
    explicit Keyboard(struct wl_keyboard *keyboard);

    ~Keyboard();

    void register_observer(KeyboardObserver *observer) {
        observers_.push_back(observer);
    }

    void unregister_observer(KeyboardObserver *observer) {
        observers_.remove(observer);
    }

    // Disallow copy and assign.
    Keyboard(const Keyboard &) = delete;

    Keyboard &operator=(const Keyboard &) = delete;

private:
    struct wl_keyboard *keyboard_;
    struct wl_surface *active_surface_{};
    struct xkb_context *xkb_context_;
    struct xkb_keymap *keymap_{};
    struct xkb_state *xkb_state_{};
    std::list<KeyboardObserver *> observers_;

    xkb_keysym_t keysym_pressed_{};
    guint key_timeout_id_{};

    int32_t key_repeat_rate_{};

    std::mutex lock_;
    uint32_t repeat_code_{};

    std::unique_ptr<EventTimer> repeat_timer_;

    static void repeat_callback(void *data);

    /**
     * @brief Handles the repeated key events for the Keyboard.
     *
     * This function is called when a key is being held down and needs to be repeated.
     *
     * @param keyboard A pointer to the Keyboard instance.
     *
     * @return TRUE if the key repeat rate is set, FALSE otherwise.
     */
    static gboolean handle_repeat(Keyboard *keyboard);

    /**
     * keyboard mapping
     *
     * This event provides a file descriptor to the client which can
     * be memory-mapped in read-only mode to provide a keyboard mapping
     * description.
     *
     * From version 7 onwards, the fd must be mapped with MAP_PRIVATE
     * by the recipient, as MAP_SHARED may fail.
     * @param format keymap format
     * @param fd keymap file descriptor
     * @param size keymap size, in bytes
     */
    static void handle_keymap(void *data,
                              struct wl_keyboard *wl_keyboard,
                              uint32_t format,
                              int32_t fd,
                              uint32_t size);

    /**
     * enter event
     *
     * Notification that this seat's keyboard focus is on a certain
     * surface.
     *
     * The compositor must send the wl_keyboard.modifiers event after
     * this event.
     * @param serial serial number of the enter event
     * @param surface surface gaining keyboard focus
     * @param keys the currently pressed keys
     */
    static void handle_enter(void *data,
                             struct wl_keyboard *wl_keyboard,
                             uint32_t serial,
                             struct wl_surface *surface,
                             struct wl_array *keys);

    /**
     * leave event
     *
     * Notification that this seat's keyboard focus is no longer on a
     * certain surface.
     *
     * The leave notification is sent before the enter notification for
     * the new focus.
     *
     * After this event client must assume that all keys, including
     * modifiers, are lifted and also it must stop key repeating if
     * there's some going on.
     * @param serial serial number of the leave event
     * @param surface surface that lost keyboard focus
     */
    static void handle_leave(void *data,
                             struct wl_keyboard *wl_keyboard,
                             uint32_t serial,
                             struct wl_surface *surface);

    /**
     * key event
     *
     * A key was pressed or released. The time argument is a
     * timestamp with millisecond granularity, with an undefined base.
     *
     * The key is a platform-specific key code that can be interpreted
     * by feeding it to the keyboard mapping (see the keymap event).
     *
     * If this event produces a change in modifiers, then the resulting
     * wl_keyboard.modifiers event must be sent after this event.
     * @param serial serial number of the key event
     * @param time timestamp with millisecond granularity
     * @param key key that produced the event
     * @param state physical state of the key
     */
    static void handle_key(void *data,
                           struct wl_keyboard *wl_keyboard,
                           uint32_t serial,
                           uint32_t time,
                           uint32_t key,
                           uint32_t state);

    /**
     * modifier and group state
     *
     * Notifies clients that the modifier and/or group state has
     * changed, and it should update its local state.
     * @param serial serial number of the modifiers event
     * @param mods_depressed depressed modifiers
     * @param mods_latched latched modifiers
     * @param mods_locked locked modifiers
     * @param group keyboard layout
     */
    static void handle_modifiers(void *data,
                                 struct wl_keyboard *wl_keyboard,
                                 uint32_t serial,
                                 uint32_t mods_depressed,
                                 uint32_t mods_latched,
                                 uint32_t mods_locked,
                                 uint32_t group);

    /**
     * repeat rate and delay
     *
     * Informs the client about the keyboard's repeat rate and delay.
     *
     * This event is sent as soon as the wl_keyboard object has been
     * created, and is guaranteed to be received by the client before
     * any key press event.
     *
     * Negative values for either rate or delay are illegal. A rate of
     * zero will disable any repeating (regardless of the value of
     * delay).
     *
     * This event can be sent later on as well with a new value if
     * necessary, so clients should continue listening for the event
     * past the creation of wl_keyboard.
     * @param rate the rate of repeating keys in characters per second
     * @param delay delay in milliseconds since key down until repeating starts
     * @since 4
     */
    static void handle_repeat_info(void *data,
                                   struct wl_keyboard *wl_keyboard,
                                   int32_t rate,
                                   int32_t delay);

    /**
     * @ingroup iface_wl_keyboard
     * @struct wl_keyboard_listener
     */
    static const struct wl_keyboard_listener keyboard_listener_;

    static inline void set_repeat_code(Keyboard *keyboard, const uint32_t repeat_code) {
        std::lock_guard lock(keyboard->lock_);
        keyboard->repeat_code_ = repeat_code;
    }
};
