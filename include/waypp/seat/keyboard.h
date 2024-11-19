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
#include <mutex>

#include <glib-2.0/glib.h>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>

class Keyboard;

class KeyboardObserver {
 public:
  enum KeyState {
    KEY_STATE_RELEASE,
    KEY_STATE_PRESS,
  };

  virtual ~KeyboardObserver() = default;

  virtual void notify_keyboard_enter(Keyboard* keyboard,
                                     wl_keyboard* wl_keyboard,
                                     uint32_t serial,
                                     wl_surface* surface,
                                     wl_array* keys) = 0;

  virtual void notify_keyboard_leave(Keyboard* keyboard,
                                     wl_keyboard* wl_keyboard,
                                     uint32_t serial,
                                     wl_surface* surface) = 0;

  virtual void notify_keyboard_keymap(Keyboard* keyboard,
                                      wl_keyboard* wl_keyboard,
                                      uint32_t format,
                                      int32_t fd,
                                      uint32_t size) = 0;

  virtual void notify_keyboard_xkb_v1_key(
      Keyboard* keyboard,
      wl_keyboard* wl_keyboard,
      uint32_t serial,
      uint32_t time,
      uint32_t xkb_scancode,
      bool keymap_key_repeats,
      uint32_t state,
      int xdg_key_symbol_count,
      const xkb_keysym_t* xdg_key_symbols) = 0;
};

class Keyboard {
 public:
  struct event_mask {
    bool enabled;
    bool all;
  };

  explicit Keyboard(wl_keyboard* keyboard, event_mask& event_mask);

  ~Keyboard();

  void register_observer(KeyboardObserver* observer,
                         void* user_data = nullptr) {
    observers_.push_back(observer);

    if (user_data) {
      user_data_ = user_data;
    }
  }

  void unregister_observer(KeyboardObserver* observer) {
    observers_.remove(observer);
  }

  void set_user_data(void* user_data) { user_data_ = user_data; }

  [[nodiscard]] void* get_user_data() const { return user_data_; }

  [[nodiscard]] int32_t get_repeat_delay() const { return repeat_.delay; }

  [[nodiscard]] int32_t get_repeat_rate() const { return repeat_.rate; }

  void set_event_mask(event_mask& event_mask);

  // Disallow copy and assign.
  Keyboard(const Keyboard&) = delete;

  Keyboard& operator=(const Keyboard&) = delete;

 private:
  struct wl_keyboard* wl_keyboard_;
  struct wl_surface* wl_surface{};
  xkb_context* xkb_context_;
  xkb_keymap* xkb_keymap_{};
  xkb_state* xkb_state_{};
  wl_keyboard_keymap_format format_{};
  std::list<KeyboardObserver*> observers_{};
  void* user_data_{};

  event_mask event_mask_{};

  struct {
    int32_t rate;
    int32_t delay;
    timer_t timer;
    uint32_t code;
    sigevent sev;
    struct sigaction sa;
    struct {
      struct wl_keyboard* wl_keyboard;
      uint32_t serial;
      uint32_t time;
      uint32_t xkb_scancode;
      int key_repeats;
      int xdg_keysym_count;
      const xkb_keysym_t* key_syms;
    } notify;
  } repeat_{};

  /**
   * @brief Handles the repeated key events for the Keyboard.
   *
   * This function is called by the kernel.
   *
   */
  static void repeat_xkb_v1_key_callback(int, siginfo_t* si, void*);

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
  static void handle_keymap(void* data,
                            wl_keyboard* wl_keyboard,
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
  static void handle_enter(void* data,
                           wl_keyboard* wl_keyboard,
                           uint32_t serial,
                           struct wl_surface* surface,
                           wl_array* keys);

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
  static void handle_leave(void* data,
                           wl_keyboard* wl_keyboard,
                           uint32_t serial,
                           struct wl_surface* surface);

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
  static void handle_key(void* data,
                         wl_keyboard* wl_keyboard,
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
  static void handle_modifiers(void* data,
                               wl_keyboard* wl_keyboard,
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
  static void handle_repeat_info(void* data,
                                 wl_keyboard* wl_keyboard,
                                 int32_t rate,
                                 int32_t delay);

  /**
   * @ingroup iface_wl_keyboard
   * @struct wl_keyboard_listener
   */
  static const wl_keyboard_listener keyboard_listener_;
};
