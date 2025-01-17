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

#include "view_manager_wayland.h"

#include <memory>

#include <linux/input.h>

#include "logging/logging.h"
#include "view_wayland.h"

ViewManagerWayland::ViewManagerWayland(
    const ViewManager::Configuration& config) {
  display_ = wl_display_connect(nullptr);
  if (!display_) {
    spdlog::critical("Unable to connect to Wayland socket.");
    exit(EXIT_FAILURE);
  }

  wm_ = std::make_shared<XdgWindowManager>(display_, config.disable_cursor);
  auto seat = wm_->get_seat();
  if (seat.has_value()) {
    seat_ = seat.value();
    seat_->register_observer(this);
  }

  spdlog::debug("XDG Window Manager Version: {}", wm_->get_version());
}

ViewManagerWayland::~ViewManagerWayland() {
  if (!views_.empty()) {
    if (views_.front()) {
      views_.front().reset();
    }
  }
  wm_.reset();
  wl_display_flush(display_);
  wl_display_disconnect(display_);
}

uint32_t ViewManagerWayland::create_view(const char* app_title,
                                         const char* app_id,
                                         int width,
                                         int height,
                                         bool fullscreen,
                                         bool maximized,
                                         bool fullscreen_ratio,
                                         bool tearing) {
  if (views_.empty()) {
    // toplevel is always index 0
    views_.emplace_back(std::make_unique<ViewWayland>(
        wm_, app_title, app_id, width, height, fullscreen, maximized,
        fullscreen_ratio, tearing));
  }
  return 0;
}

bool ViewManagerWayland::poll_events() {
  /// display_dispatch is blocking
  return (views_[0]->is_valid() && wm_->display_dispatch() != -1);
}

void ViewManagerWayland::quit() {
  if (!views_.empty()) {
    if (views_.front()) {
      views_.front()->close();
    }
  }
}

void ViewManagerWayland::toggle_fullscreen() {
  views_[0]->toggle_fullscreen();
}

void ViewManagerWayland::notify_seat_capabilities(Seat* seat,
                                                  wl_seat* /* seat */,
                                                  uint32_t /* caps */) {
  if (seat) {
    auto keyboard = seat->get_keyboard();
    if (keyboard.has_value()) {
      keyboard.value()->register_observer(this, this);
    }
    auto pointer = seat->get_pointer();
    if (pointer.has_value()) {
      pointer.value()->register_observer(this, this);
    }
  }
}

void ViewManagerWayland::notify_seat_name(Seat* /* seat */,
                                          wl_seat* /* seat */,
                                          const char* /* name */) {}

void ViewManagerWayland::notify_keyboard_enter(Keyboard* /* keyboard */,
                                               wl_keyboard* /* wl_keyboard */,
                                               uint32_t /* serial */,
                                               wl_surface* /* surface */,
                                               wl_array* /* keys */) {}

void ViewManagerWayland::notify_keyboard_leave(Keyboard* /* keyboard */,
                                               wl_keyboard* /* wl_keyboard */,
                                               uint32_t /* serial */,
                                               wl_surface* /* surface */) {}

void ViewManagerWayland::notify_keyboard_keymap(Keyboard* /* keyboard */,
                                                wl_keyboard* /* wl_keyboard */,
                                                uint32_t /* format */,
                                                int32_t /* fd */,
                                                uint32_t /* size */) {}

void ViewManagerWayland::notify_keyboard_xkb_v1_key(
    Keyboard* keyboard,
    wl_keyboard* /* wl_keyboard */,
    uint32_t /* serial */,
    uint32_t /* time */,
    uint32_t /* xkb_scancode */,
    bool /* key_repeats */,
    uint32_t state,
    int xdg_key_symbol_count,
    const xkb_keysym_t* xdg_key_symbols) {
  if (xdg_key_symbol_count && state == KeyState::KEY_STATE_PRESS) {
    auto view_manager =
        static_cast<ViewManagerWayland*>(keyboard->get_user_data());

    switch (xdg_key_symbols[0]) {
      case XKB_KEY_Escape:
        spdlog::info("Quit");
        view_manager->quit();
        break;
      case XKB_KEY_F11:
        spdlog::info("Toggle fullscreen");
        view_manager->toggle_fullscreen();
        break;
    }
  }
}

void ViewManagerWayland::notify_pointer_enter(Pointer* pointer,
                                              wl_pointer* /* pointer */,
                                              uint32_t serial,
                                              wl_surface* /* surface */,
                                              double /* sx */,
                                              double /* sy */) {
  pointer->set_cursor(serial, "left_ptr");
}

void ViewManagerWayland::notify_pointer_leave(Pointer* /* pointer */,
                                              wl_pointer* /* pointer */,
                                              uint32_t /* serial */,
                                              wl_surface* /* surface */) {}

void ViewManagerWayland::notify_pointer_motion(Pointer* /* pointer */,
                                               wl_pointer* /* pointer */,
                                               uint32_t /* time */,
                                               double /* sx */,
                                               double /* sy */) {}

void ViewManagerWayland::notify_pointer_button(Pointer* pointer,
                                               wl_pointer* /* pointer  */,
                                               uint32_t serial,
                                               uint32_t /* time */,
                                               uint32_t button,
                                               uint32_t state) {
  if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
    uint32_t edge = views_.front()->check_edge_resize(pointer->get_xy());
    if (edge != XDG_TOPLEVEL_RESIZE_EDGE_NONE) {
      views_.front()->resize(seat_->get_seat(), serial, edge);
    }
  }
}

void ViewManagerWayland::notify_pointer_axis(Pointer* /* pointer */,
                                             wl_pointer* /* pointer */,
                                             uint32_t /* time */,
                                             uint32_t /* axis */,
                                             double /* value */) {}

void ViewManagerWayland::notify_pointer_frame(Pointer* /* pointer */,
                                              wl_pointer* /* pointer */) {};

void ViewManagerWayland::notify_pointer_axis_source(
    Pointer* /* pointer */,
    wl_pointer* /* pointer */,
    uint32_t /* axis_source */) {};

void ViewManagerWayland::notify_pointer_axis_stop(Pointer* /* pointer */,
                                                  wl_pointer* /* pointer */,
                                                  uint32_t /* time */,
                                                  uint32_t /* axis */) {};

void ViewManagerWayland::notify_pointer_axis_discrete(Pointer* /* pointer */,
                                                      wl_pointer* /*pointer */,
                                                      uint32_t /* axis */,
                                                      int32_t /* discrete */) {}
