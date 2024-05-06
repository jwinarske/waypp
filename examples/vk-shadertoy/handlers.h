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

#pragma once

#include <random>

#include "window/xdg_toplevel.h"

class Handlers final : public SeatObserver, public PointerObserver, public KeyboardObserver {
public:
    Handlers();

    ~Handlers() override = default;

private:
    std::random_device rd_;
    std::mt19937 gen_;

    void notify_seat_capabilities(Seat *seat, wl_seat *, uint32_t) override;

    void notify_seat_name(Seat *, wl_seat *, const char *name) override;

    void notify_keyboard_enter(Keyboard *, wl_keyboard *, uint32_t, wl_surface *, wl_array *) override;

    void notify_keyboard_leave(Keyboard *, wl_keyboard *, uint32_t, wl_surface *) override;

    void notify_keyboard_keymap(Keyboard *, wl_keyboard *, uint32_t, int32_t, uint32_t) override;

    void notify_keyboard_xkb_v1_key(Keyboard *, wl_keyboard *, uint32_t, uint32_t, uint32_t, bool, uint32_t, int,
                                    const xkb_keysym_t *) override;

    void notify_pointer_enter(Pointer *, wl_pointer *, uint32_t, wl_surface *, double, double) override;

    void notify_pointer_leave(Pointer *, wl_pointer *, uint32_t, wl_surface *) override;

    void notify_pointer_motion(Pointer *, wl_pointer *, uint32_t, double, double) override;

    void notify_pointer_button(Pointer *, wl_pointer *, uint32_t, uint32_t, uint32_t, uint32_t state) override;

    void notify_pointer_axis(Pointer *, wl_pointer *, uint32_t, uint32_t, wl_fixed_t) override;

    void notify_pointer_frame(Pointer *, wl_pointer *) override;

    void notify_pointer_axis_source(Pointer *, wl_pointer *, uint32_t axis_source) override;

    void notify_pointer_axis_stop(Pointer *, wl_pointer *, uint32_t, uint32_t axis) override;

    void notify_pointer_axis_discrete(Pointer *, wl_pointer *, uint32_t axis, int32_t discrete) override;
};
