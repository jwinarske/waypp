/*
 * Copyright © 2024 Joel Winarske
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "handlers.h"

#include "logging.h"

static std::vector<std::string> gCursors = Pointer::get_available_cursors();

Handlers::Handlers() : gen_(rd_()) {}

void Handlers::notify_seat_capabilities(Seat* seat,
                                        wl_seat* /* seat */,
                                        uint32_t /* caps */) {
  if (seat) {
    auto keyboard = seat->get_keyboard();
    if (keyboard.has_value()) {
      keyboard.value()->register_observer(this);
    }

    auto pointer = seat->get_pointer();
    if (pointer.has_value()) {
      pointer.value()->register_observer(this);
    }
  }
}

void Handlers::notify_seat_name(Seat* /* seat */,
                                wl_seat* /* seat */,
                                const char* name) {
  spdlog::info("Seat: {}", name);
}

void Handlers::notify_keyboard_enter(Keyboard* /* keyboard */,
                                     wl_keyboard* /* wl_keyboard */,
                                     uint32_t serial,
                                     wl_surface* surface,
                                     wl_array* /* keys */) {
  spdlog::info("Keyboard Enter: serial: {}, surface: {}", serial,
               fmt::ptr(surface));
}

void Handlers::notify_keyboard_leave(Keyboard* /* keyboard */,
                                     wl_keyboard* /* wl_keyboard */,
                                     uint32_t serial,
                                     wl_surface* surface) {
  spdlog::info("Keyboard Leave: serial: {}, surface: {}", serial,
               fmt::ptr(surface));
}

void Handlers::notify_keyboard_keymap(Keyboard* /* keyboard */,
                                      wl_keyboard* /* wl_keyboard */,
                                      uint32_t format,
                                      int32_t fd,
                                      uint32_t size) {
  spdlog::info("Keymap: format: {}, fd: {}, size: {}", format, fd, size);
}

void Handlers::notify_keyboard_xkb_v1_key(Keyboard* /* keyboard */,
                                          wl_keyboard* /* wl_keyboard */,
                                          uint32_t serial,
                                          uint32_t time,
                                          uint32_t xkb_scancode,
                                          bool key_repeats,
                                          uint32_t state,
                                          int xdg_key_symbol_count,
                                          const xkb_keysym_t* xdg_key_symbols) {
  spdlog::info(
      "Key: serial: {}, time: {}, xkb_scancode: 0x{:X}, key_repeats: {}, "
      "state: {}, xdg_keysym_count: {}, syms_out[0]: 0x{:X}",
      serial, time, xkb_scancode, key_repeats,
      state == KeyState::KEY_STATE_PRESS ? "press" : "release",
      xdg_key_symbol_count, xdg_key_symbols[0]);
}

void Handlers::notify_pointer_enter(Pointer* pointer,
                                    wl_pointer* /* pointer */,
                                    uint32_t serial,
                                    wl_surface* surface,
                                    double sx,
                                    double sy) {
  spdlog::info("Pointer Enter: serial: {}, surface: {}, x: {}, y: {}", serial,
               fmt::ptr(surface), sx, sy);

  if (gCursors.size() > 1) {
    std::uniform_int_distribution<size_t> distribution(0, gCursors.size() - 1);
    pointer->set_cursor(serial, gCursors[distribution(gen_)].c_str());
  } else {
    pointer->set_cursor(serial, "crosshair");
  }
}

void Handlers::notify_pointer_leave(Pointer* /* pointer */,
                                    wl_pointer* /* pointer */,
                                    uint32_t serial,
                                    wl_surface* surface) {
  spdlog::info("Pointer Leave: serial: {}, surface: {}", serial,
               fmt::ptr(surface));
}

void Handlers::notify_pointer_motion(Pointer* /* pointer  */,
                                     wl_pointer* /* pointer */,
                                     uint32_t time,
                                     double sx,
                                     double sy) {
  spdlog::info("Pointer: time: {}, x: {}, y: {}", time, sx, sy);
}

void Handlers::notify_pointer_button(Pointer* /* pointer */,
                                     wl_pointer* /* pointer  */,
                                     uint32_t serial,
                                     uint32_t time,
                                     uint32_t button,
                                     uint32_t state) {
  spdlog::info("Pointer Button: pointer: {}, time: {}, button: {}, state: {}",
               serial, time, button, state);
}

void Handlers::notify_pointer_axis(Pointer* /* pointer */,
                                   wl_pointer* /* pointer */,
                                   uint32_t time,
                                   uint32_t axis,
                                   wl_fixed_t value) {
  spdlog::info("Pointer Axis: time: {}, axis: {}, value: {}", time, axis,
               value);
}

void Handlers::notify_pointer_frame(Pointer* /* pointer */,
                                    wl_pointer* /* pointer */) {
  spdlog::info("Pointer Frame");
};

void Handlers::notify_pointer_axis_source(Pointer* /* pointer */,
                                          wl_pointer* /* pointer */,
                                          uint32_t axis_source) {
  spdlog::info("Pointer Axis Source: axis_source: {}", axis_source);
};

void Handlers::notify_pointer_axis_stop(Pointer* /* pointer */,
                                        wl_pointer* /* pointer */,
                                        uint32_t /* time */,
                                        uint32_t axis) {
  spdlog::info("Pointer Axis Stop: axis: {}", axis);
};

void Handlers::notify_pointer_axis_discrete(Pointer* /* pointer */,
                                            wl_pointer* /*pointer */,
                                            uint32_t axis,
                                            int32_t discrete) {
  spdlog::info("Pointer Axis Discrete: axis: {}, discrete: {}", axis, discrete);
}
