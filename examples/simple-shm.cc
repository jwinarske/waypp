/*
 * Copyright © 2024 Joel Winarske
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2010 Intel Corporation
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

#include <csignal>
#include <random>

#include <linux/input.h>
#include <cxxopts.hpp>

#include <wayland-client.h>
#include "logging.h"
#include "window/xdg_toplevel.h"

struct Configuration {
    int width;
    int height;
    bool disable_cursor;
    bool fullscreen;
    bool maximized;
    bool fullscreen_ratio;
    bool tearing;
};

static constexpr int kResizeMargin = 12;

static volatile bool gRunning = true;

static std::vector<std::string> gCursors = Pointer::get_available_cursors();

/**
 * @brief Signal handler function to handle signals.
 *
 * This function is a signal handler for handling signals. It sets the value of
 * keep_running to false, which will stop the program from running. The function
 * does not take any input parameters.
 *
 * @param signal The signal number. This parameter is not used by the function.
 *
 * @return void
 */
void handle_signal(int signal) {
    if (signal == SIGINT) {
        gRunning = false;
    }
}

static void paint_pixels(void *image,
                         int padding,
                         int width,
                         int height,
                         uint32_t time) {
    auto pixel = static_cast<uint32_t *>(image);
    int half_h = padding + (height - padding * 2) / 2;
    int half_w = padding + (width - padding * 2) / 2;
    int ir, or_;
    int y;

    /// Squared radii thresholds
    or_ = (half_w < half_h ? half_w : half_h) - 8;
    ir = or_ - 32;
    or_ *= or_;
    ir *= ir;

    pixel += padding * width;
    for (y = padding; y < height - padding; y++) {
        int x;
        int y2 = (y - half_h) * (y - half_h);

        pixel += padding;
        for (x = padding; x < width - padding; x++) {
            uint32_t v;

            /// Squared distance from center
            int r2 = (x - half_w) * (x - half_w) + y2;

            if (r2 < ir)
                v = (static_cast<uint32_t>(r2 / 32) + time / 64) * 0x0080401;
            else if (r2 < or_)
                v = (static_cast<uint32_t>(y) + time / 32) * 0x0080401;
            else
                v = (static_cast<uint32_t>(x) + time / 16) * 0x0080401;
            v &= 0x00ffffff;

            /// Cross if compositor uses X from XRGB as alpha
            if (abs(x - y) > 6 && abs(x + y - height) > 6)
                v |= 0xff000000;

            *pixel++ = v;
        }

        pixel += padding;
    }
}

void draw_frame(void *data, const uint32_t time) {
    auto window = static_cast<Window *>(data);

    auto buffer = window->next_buffer();
    if (!buffer) {
        spdlog::error("Failed to acquire a buffer");
        exit(EXIT_FAILURE);
    }

    paint_pixels(buffer->get_shm_data(), 20, window->get_width(),
                 window->get_height(), time);

    wl_surface_attach(window->get_surface(), buffer->get_wl_buffer(), 0, 0);
    wl_surface_damage(window->get_surface(), 20, 20, window->get_width() - 40,
                      window->get_height() - 40);

    buffer->set_busy();
}

class App : public PointerObserver,
            public KeyboardObserver,
            public SeatObserver {
public:
    explicit App(const Configuration &config)
            : logging_(std::make_unique<Logging>()), gen_(rd_()) {
        wl_display_ = wl_display_connect(nullptr);
        if (!wl_display_) {
            spdlog::critical("Unable to connect to Wayland socket.");
            exit(EXIT_FAILURE);
        }

        wm_ = std::make_unique<XdgWindowManager>(wl_display_, config.disable_cursor);
        auto seat = wm_->get_seat();
        if (seat.has_value()) {
            seat_ = seat.value();
            seat_->register_observer(this);
        }

        spdlog::info("XDG Window Manager Version: {}", wm_->get_version());

        toplevel_ = wm_->create_top_level(
                "simple-shm", "org.freedesktop.gitlab.jwinarske.waypp.simple_shm",
                config.width, config.height, kResizeMargin, 2, WL_SHM_FORMAT_XRGB8888,
                config.fullscreen, config.maximized, config.fullscreen_ratio,
                config.tearing, draw_frame);
        spdlog::info("XDG Window Version: {}", toplevel_->get_version());

        /// paint padding
        toplevel_->set_surface_damage(0, 0, config.width, config.height);
        toplevel_->start_frame_callbacks();
    }

    ~App() override {
        toplevel_.reset();
        wm_.reset();

        if (wl_display_) {
            wl_display_flush(wl_display_);
            wl_display_disconnect(wl_display_);
        }
    }

    bool run() {
        /// display_dispatch is blocking
        return (toplevel_->is_valid() && wm_->display_dispatch() != -1);
    }

    void notify_seat_capabilities(Seat *seat,
                                  wl_seat * /* seat */,
                                  uint32_t /* caps */) override {
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

    void notify_seat_name(Seat * /* seat */,
                          wl_seat * /* seat */,
                          const char *name) override {
        spdlog::info("Seat: {}", name);
    }

    void notify_keyboard_enter(Keyboard * /* keyboard */,
                               wl_keyboard * /* wl_keyboard */,
                               uint32_t serial,
                               wl_surface *surface,
                               wl_array * /* keys */) override {
        spdlog::info("Keyboard Enter: serial: {}, surface: {}", serial,
                     fmt::ptr(surface));
    }

    void notify_keyboard_leave(Keyboard * /* keyboard */,
                               wl_keyboard * /* wl_keyboard */,
                               uint32_t serial,
                               wl_surface *surface) override {
        spdlog::info("Keyboard Leave: serial: {}, surface: {}", serial,
                     fmt::ptr(surface));
    }

    void notify_keyboard_keymap(Keyboard * /* keyboard */,
                                wl_keyboard * /* wl_keyboard */,
                                uint32_t format,
                                int32_t fd,
                                uint32_t size) override {
        spdlog::info("Keymap: format: {}, fd: {}, size: {}", format, fd, size);
    }

    void notify_keyboard_xkb_v1_key(
            Keyboard * /* keyboard */,
            wl_keyboard * /* wl_keyboard */,
            uint32_t serial,
            uint32_t time,
            uint32_t xkb_scancode,
            bool key_repeats,
            uint32_t state,
            int xdg_key_symbol_count,
            const xkb_keysym_t *xdg_key_symbols) override {
        spdlog::info(
                "Key: serial: {}, time: {}, xkb_scancode: 0x{:X}, key_repeats: {}, "
                "state: {}, xdg_keysym_count: {}, syms_out[0]: 0x{:X}",
                serial, time, xkb_scancode, key_repeats,
                state == KeyState::KEY_STATE_PRESS ? "press" : "release",
                xdg_key_symbol_count, xdg_key_symbols[0]);
    }

    void notify_pointer_enter(Pointer *pointer,
                              wl_pointer * /* pointer */,
                              uint32_t serial,
                              wl_surface *surface,
                              double sx,
                              double sy) override {
        spdlog::info("Pointer Enter: serial: {}, surface: {}, x: {}, y: {}", serial,
                     fmt::ptr(surface), sx, sy);

        if (gCursors.size() > 1) {
            std::uniform_int_distribution<size_t> distribution(0,
                                                               gCursors.size() - 1);
            pointer->set_cursor(serial, gCursors[distribution(gen_)].c_str());
        } else {
            pointer->set_cursor(serial, "crosshair");
        }
    }

    void notify_pointer_leave(Pointer * /* pointer */,
                              wl_pointer * /* pointer */,
                              uint32_t serial,
                              wl_surface *surface) override {
        spdlog::info("Pointer Leave: serial: {}, surface: {}", serial,
                     fmt::ptr(surface));
    }

    void notify_pointer_motion(Pointer * /* pointer  */,
                               wl_pointer * /* pointer */,
                               uint32_t time,
                               double sx,
                               double sy) override {
        spdlog::info("Pointer: time: {}, x: {}, y: {}", time, sx, sy);
    }

    void notify_pointer_button(Pointer * pointer,
                               wl_pointer * /* pointer  */,
                               uint32_t serial,
                               uint32_t time,
                               uint32_t button,
                               uint32_t state) override {
        spdlog::info("Pointer Button: pointer: {}, time: {}, button: {}, state: {}",
                     serial, time, button, state);
        if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
            auto edge = toplevel_->check_edge_resize(pointer->get_xy());
            if (edge != XDG_TOPLEVEL_RESIZE_EDGE_NONE) {
                toplevel_->resize(seat_->get_seat(), serial, edge);
            }
        }
    }

    void notify_pointer_axis(Pointer * /* pointer */,
                             wl_pointer * /* pointer */,
                             uint32_t time,
                             uint32_t axis,
                             double value) override {
        spdlog::info("Pointer Axis: time: {}, axis: {}, value: {}", time, axis,
                     value);
    }

    void notify_pointer_frame(Pointer * /* pointer */,
                              wl_pointer * /* pointer */) override {
        spdlog::info("Pointer Frame");
    };

    void notify_pointer_axis_source(Pointer * /* pointer */,
                                    wl_pointer * /* pointer */,
                                    uint32_t axis_source) override {
        spdlog::info("Pointer Axis Source: axis_source: {}", axis_source);
    };

    void notify_pointer_axis_stop(Pointer * /* pointer */,
                                  wl_pointer * /* pointer */,
                                  uint32_t /* time */,
                                  uint32_t axis) override {
        spdlog::info("Pointer Axis Stop: axis: {}", axis);
    };

    void notify_pointer_axis_discrete(Pointer * /* pointer */,
                                      wl_pointer * /*pointer */,
                                      uint32_t axis,
                                      int32_t discrete) override {
        spdlog::info("Pointer Axis Discrete: axis: {}, discrete: {}", axis,
                     discrete);
    }

private:
    struct wl_display *wl_display_;
    std::unique_ptr<Logging> logging_;
    std::shared_ptr<XdgWindowManager> wm_;
    Seat *seat_{};
    std::shared_ptr<XdgTopLevel> toplevel_;
    std::random_device rd_;
    std::mt19937 gen_;
};

int main(int argc, char **argv) {
    std::signal(SIGINT, handle_signal);

    cxxopts::Options options("simple-shm", "Weston simple-shm example");
    options.add_options()
            // clang-format off
            ("w,width", "Set width", cxxopts::value<int>()->default_value("250"))
            ("h,height", "Set height", cxxopts::value<int>()->default_value("250"))
            ("c,disable-cursor", "Disable Cursor")
            ("f,fullscreen", "Run in fullscreen mode")
            ("m,maximized", "Run in maximized mode")
            ("r,fullscreen-ratio", "Use fixed width/height ratio when run in fullscreen mode")
            ("t,tearing", "Enable tearing via the tearing_control protocol");

    // clang-format on
    auto result = options.parse(argc, argv);

    App app({
                    .width = result["width"].as<int>(),
                    .height = result["height"].as<int>(),
                    .disable_cursor = result["disable-cursor"].as<bool>(),
                    .fullscreen = result["fullscreen"].as<bool>(),
                    .maximized = result["maximized"].as<bool>(),
                    .fullscreen_ratio = result["fullscreen-ratio"].as<bool>(),
                    .tearing = result["tearing"].as<bool>(),
            });

    while (gRunning && app.run()) {}

    return EXIT_SUCCESS;
}