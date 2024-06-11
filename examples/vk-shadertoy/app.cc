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

#include "app.h"

#include <linux/input.h>


void App::draw_frame(void *data, const uint32_t time) {
    auto window = static_cast<Window *>(data);
    auto shader_toy = static_cast<ShaderToy *>(window->get_user_data());
    shader_toy->draw_frame(time);
}

App::App(const Configuration &config) : logging_(std::make_unique<Logging>()) {
    spdlog::info("{}", kAppTitle);

    display_ = wl_display_connect(nullptr);
    if (!display_) {
        spdlog::critical("Unable to connect to Wayland socket.");
        exit(EXIT_FAILURE);
    }

    shader_toy_ = std::make_unique<ShaderToy>();
    wm_ = std::make_shared<XdgWindowManager>(display_, config.disable_cursor);
    auto seat = wm_->get_seat();
    if (seat.has_value()) {
        seat.value()->register_observer(this, this);
    }

    spdlog::debug("XDG Window Manager Version: {}", wm_->get_version());

    toplevel_ = wm_->create_top_level(
            kAppTitle, kAppId, config.width, config.height, kResizeMargin, 0, 0, config.fullscreen,
            config.maximized, true, config.tearing, draw_frame);
    spdlog::debug("XDG Window Version: {}", toplevel_->get_version());

    shader_toy_->init(config.width,
                      config.height,
                      display_,
                      toplevel_->get_surface(),
                      config.dev_index,
                      config.use_gpu_idx,
                      config.debug,
                      config.reload_shaders,
                      static_cast<VkPresentModeKHR>(config.present_mode));


    /// paint padding
    toplevel_->set_surface_damage(0, 0, config.width, config.height);

    /// start frame callbacks with user_data pointing to shadertoy
    toplevel_->start_frame_callbacks(shader_toy_.get());
}

App::~App() {
    toplevel_.reset();
    wm_.reset();
    wl_display_flush(display_);
    wl_display_flush(display_);
}

bool App::run() {
    /// display_dispatch is blocking
    return (toplevel_->is_valid() && wm_->display_dispatch() != -1);
}

void App::notify_seat_capabilities(Seat *seat,
                                   wl_seat * /* seat */,
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

void App::notify_seat_name(Seat * /* seat */,
                           wl_seat * /* seat */,
                           const char * /* name */) {
}

void App::notify_keyboard_enter(Keyboard * /* keyboard */,
                                wl_keyboard * /* wl_keyboard */,
                                uint32_t /* serial */,
                                wl_surface * /* surface */,
                                wl_array * /* keys */) {
}

void App::notify_keyboard_leave(Keyboard * /* keyboard */,
                                wl_keyboard * /* wl_keyboard */,
                                uint32_t /* serial */,
                                wl_surface * /* surface */) {
}

void App::notify_keyboard_keymap(Keyboard * /* keyboard */,
                                 wl_keyboard * /* wl_keyboard */,
                                 uint32_t /* format */,
                                 int32_t /* fd */,
                                 uint32_t /* size */) {
}

void App::notify_keyboard_xkb_v1_key(Keyboard *keyboard,
                                     wl_keyboard * /* wl_keyboard */,
                                     uint32_t /* serial */,
                                     uint32_t /* time */,
                                     uint32_t /* xkb_scancode */,
                                     bool /* key_repeats */,
                                     uint32_t state,
                                     int xdg_key_symbol_count,
                                     const xkb_keysym_t *xdg_key_symbols) {

    if (xdg_key_symbol_count && state == KeyState::KEY_STATE_PRESS) {

        auto app = static_cast<App *>(keyboard->get_user_data());
        auto shader_toy = app->shader_toy_.get();

        switch (xdg_key_symbols[0]) {
            case XKB_KEY_Escape:
                spdlog::info("Quit");
                app->toplevel_->close();
                break;
            case XKB_KEY_space:
                spdlog::info("Toggle Pause");
                shader_toy->toggle_pause();
                break;
            case XKB_KEY_0:
                spdlog::info("Toggle Draw Debug");
                shader_toy->toggle_draw_debug();
                break;
            case XKB_KEY_1:
                spdlog::info("Toggle FPS Lock");
                shader_toy->toggle_fps_lock();
                break;
            case XKB_KEY_z:
                spdlog::info("Screen shot");
                shader_toy->screen_shot();
                break;
            case XKB_KEY_f:
            case XKB_KEY_F11:
                spdlog::info("Toggle fullscreen");
                app->toggle_fullscreen();
                break;
        }
    }
}

void App::notify_pointer_enter(Pointer *pointer,
                               wl_pointer * /* pointer */,
                               uint32_t serial,
                               wl_surface * /* surface */,
                               double /* sx */,
                               double /* sy */) {
    pointer->set_cursor(serial, "left_ptr");
}

void App::notify_pointer_leave(Pointer * /* pointer */,
                               wl_pointer * /* pointer */,
                               uint32_t /* serial */,
                               wl_surface * /* surface */) {
}

void App::notify_pointer_motion(Pointer *pointer,
                                wl_pointer * /* pointer */,
                                uint32_t /* time */,
                                double sx,
                                double sy) {

    auto app = static_cast<App *>(pointer->get_user_data());
    auto shader_toy = app->shader_toy_.get();
    auto os_window = shader_toy->get_app_os_window();
    os_window->app_data.iMouse[0] = sx;
    os_window->app_data.iMouse[1] = os_window->app_data.iResolution[1] - sy;
}

void App::notify_pointer_button(Pointer *pointer,
                                wl_pointer * /* pointer  */,
                                uint32_t /* serial */,
                                uint32_t /* time */,
                                uint32_t button,
                                uint32_t state) {

    auto app = static_cast<App *>(pointer->get_user_data());
    auto shader_toy = app->shader_toy_.get();
    auto os_window = shader_toy->get_app_os_window();

    switch (button) {
        case BTN_LEFT:
            os_window->app_data.iMouse_click[0] = state;
            if (state) {
                os_window->app_data.iMouse_lclick[0] = static_cast<int>(os_window->app_data.iMouse[0]);
                os_window->app_data.iMouse_lclick[1] = static_cast<int>(os_window->app_data.iResolution[1] -
                                                                        os_window->app_data.iMouse[1]);
            } else {
                os_window->app_data.iMouse_lclick[0] = -os_window->app_data.iMouse_lclick[0];
                os_window->app_data.iMouse_lclick[1] = -os_window->app_data.iMouse_lclick[1];
            }
            break;
        case BTN_MIDDLE:
            break;
        case BTN_RIGHT:
            os_window->app_data.iMouse_click[1] = state;
            if (state) {
                os_window->app_data.iMouse_rclick[0] = static_cast<int>(os_window->app_data.iMouse[0]);
                os_window->app_data.iMouse_rclick[1] = static_cast<int>(os_window->app_data.iResolution[1] -
                                                                        os_window->app_data.iMouse[1]);
            } else {
                os_window->app_data.iMouse_rclick[0] = -os_window->app_data.iMouse_rclick[0];
                os_window->app_data.iMouse_rclick[1] = -os_window->app_data.iMouse_rclick[1];
            }
            break;
        default:
            break;
    }
}

void App::notify_pointer_axis(Pointer * /* pointer */,
                              wl_pointer * /* pointer */,
                              uint32_t /* time */,
                              uint32_t /* axis */,
                              double /* value */) {
}

void App::notify_pointer_frame(Pointer * /* pointer */,
                               wl_pointer * /* pointer */) {
};

void App::notify_pointer_axis_source(Pointer * /* pointer */,
                                     wl_pointer * /* pointer */,
                                     uint32_t /* axis_source */) {
};

void App::notify_pointer_axis_stop(Pointer * /* pointer */,
                                   wl_pointer * /* pointer */,
                                   uint32_t /* time */,
                                   uint32_t /* axis */) {
};

void App::notify_pointer_axis_discrete(Pointer * /* pointer */,
                                       wl_pointer * /*pointer */,
                                       uint32_t /* axis */,
                                       int32_t /* discrete */) {
}
