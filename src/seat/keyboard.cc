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

#include <iostream>

#include <wayland-client.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>


/**
 * @class Keyboard
 * @brief Represents a keyboard device
 *
 * The Keyboard class provides a wrapper for a keyboard device,
 * which interacts with the Wayland compositor.
 */
Keyboard::Keyboard(struct wl_keyboard *keyboard) : keyboard_(keyboard),
                                                   xkb_context_(xkb_context_new(XKB_CONTEXT_NO_FLAGS)) {
    wl_keyboard_add_listener(keyboard, &listener_, this);
}

/**
 * @class Keyboard
 * @brief Represents a keyboard input device.
 *
 * The Keyboard class manages the interaction with a Wayland keyboard input device.
 */
Keyboard::~Keyboard() {
    wl_keyboard_release(keyboard_);
    wl_keyboard_destroy(keyboard_);
}

/**
 * @brief Handles the enter event from the keyboard
 *
 * This function is called when the keyboard enters a surface.
 *
 * @param data The user data associated with the keyboard
 * @param keyboard The keyboard object
 * @param serial The serial of the event
 * @param surface The surface the keyboard entered
 * @param keys The keys array
 *
 * @return None
 */
void Keyboard::handle_enter(void *data,
                            struct wl_keyboard * /* keyboard */,
                            uint32_t /* serial */,
                            struct wl_surface *surface,
                            struct wl_array * /* keys */) {
    std::cerr << "handle_enter" << std::endl;
    const auto obj = static_cast<Keyboard *>(data);
    obj->active_surface_ = surface;
}

/**
 * @brief This function handles the leave event for the keyboard.
 *
 * When the keyboard leaves a surface, this function is called to update the
 * active surface to nullptr.
 *
 * @param data A pointer to the Keyboard object.
 * @param keyboard A pointer to the wl_keyboard object.
 * @param serial The serial of the event.
 * @param surface A pointer to the wl_surface object.
 */
void Keyboard::handle_leave(void *data,
                            struct wl_keyboard * /* keyboard */,
                            uint32_t /* serial */,
                            struct wl_surface * /* surface */) {
    std::cerr << "handle_leave" << std::endl;
    const auto obj = static_cast<Keyboard *>(data);
    obj->active_surface_ = nullptr;
}

/**
 * @class Keyboard
 * @brief The Keyboard class handles keyboard input events.
 *
 * This class provides functionality to handle keyboard events such as keymap changes.
 */
void Keyboard::handle_keymap(void *data,
                             struct wl_keyboard * /* keyboard */,
                             uint32_t /* format */,
                             int fd,
                             uint32_t size) {
    const auto obj = static_cast<Keyboard *>(data);
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

/**
 * @brief Handles key events from the keyboard.
 *
 * This function is called when a key is pressed or released on the keyboard.
 *
 * @param data The pointer to the Keyboard instance.
 * @param keyboard The wl_keyboard object associated with the event.
 * @param serial The serial number of the event.
 * @param time The timestamp of the event.
 * @param key The key that was pressed or released.
 * @param state The state of the key (pressed or released).
 */
void Keyboard::handle_key(void *data,
                          struct wl_keyboard * /* keyboard */,
                          uint32_t /* serial */,
                          uint32_t /* time */,
                          uint32_t key,
                          uint32_t state) {
    const auto obj = static_cast<Keyboard *>(data);

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
                std::cerr << "xkb keysym: 0x" << std::hex << key_symbols[i] << std::dec << std::endl;
            }
        }
    }

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (xkb_keymap_key_repeats(obj->keymap_, xkb_scancode)) {
        }
    } else if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
    }
}

/***************************************************************************/
void Keyboard::handle_modifiers(void *data,
                                struct wl_keyboard * /* keyboard */,
                                uint32_t /* serial */,
                                uint32_t mods_depressed,
                                uint32_t mods_latched,
                                uint32_t mods_locked,
                                uint32_t group) {
    const auto obj = static_cast<Keyboard *>(data);
    xkb_state_update_mask(obj->xkb_state_, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

/**
 * @brief Handles the repeated key events for the Keyboard.
 *
 * This function is called when a key is being held down and needs to be repeated.
 *
 * @param keyboard A pointer to the Keyboard instance.
 *
 * @return TRUE if the key repeat rate is set, FALSE otherwise.
 */
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

/**
* @brief Handles repeat info for the Keyboard class.
*
* This function is called when repeat rate and delay of key repeats are received.
* It creates a timeout to call the handle_repeat function at the specified delay.
*
* @param data A pointer to the Keyboard object.
* @param wl_keyboard A pointer to the wl_keyboard object.
* @param rate The repeat rate of key events in milliseconds.
* @param delay The delay before key repeat starts in milliseconds.
*/
void Keyboard::handle_repeat_info(void *data,
                                  struct wl_keyboard * /* wl_keyboard */,
                                  int32_t rate,
                                  int32_t delay) {
    const auto obj = static_cast<Keyboard *>(data);
    obj->key_timeout_id_ = g_timeout_add(static_cast<guint>(delay), reinterpret_cast<GSourceFunc>(handle_repeat), obj);
    obj->key_repeat_rate_ = rate;
    obj->key_timeout_id_ = g_timeout_add(static_cast<guint>(delay),
                                         reinterpret_cast<GSourceFunc>(handle_repeat), obj);
}

const struct wl_keyboard_listener Keyboard::listener_ = {
        .keymap = handle_keymap,
        .enter = handle_enter,
        .leave = handle_leave,
        .key = handle_key,
        .modifiers = handle_modifiers,
        .repeat_info = handle_repeat_info
};