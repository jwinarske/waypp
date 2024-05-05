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

#include <thread>

void App::draw_frame(void * /* data */, const uint32_t /* time */) {
    // auto window = static_cast<Window *>(data);
}

App::App(const Configuration &config) : handlers_(std::make_unique<Handlers>()), logging_(std::make_unique<Logging>()) {

    spdlog::info("{}", kAppTitle);

    std::thread t1([&] {
        backend_ = std::make_unique<VulkanBackend>(kAppId, config.debug_enable);
    });

    std::thread t2([&] {
        wm_ = std::make_unique<XdgWindowManager>(config.disable_cursor);
        auto seat = wm_->get_seat();
        if (seat.has_value()) {
            seat.value()->register_observer(handlers_.get());
        }

        spdlog::debug("XDG Window Manager Version: {}", wm_->get_version());

        toplevel_ = wm_->create_top_level(kAppTitle,
                                          kAppId,
                                          config.width,
                                          config.height,
                                          0,
                                          0,
                                          config.fullscreen,
                                          config.maximized,
                                          config.fullscreen_ratio,
                                          config.tearing,
                                          draw_frame
        );
        spdlog::debug("XDG Window Version: {}", toplevel_->get_version());
    });

    t1.join();
    t2.join();

    backend_->CreateSurface(wm_->get_display(), toplevel_->get_surface(), config.width, config.height);

    /// paint padding
    toplevel_->set_surface_damage(0, 0, config.width, config.height);
    toplevel_->start_frame_callbacks();
}

App::~App() {
    toplevel_->stop_frame_callbacks();
}

bool App::run() {
    /// display_dispatch is blocking
    return (toplevel_->is_valid() && wm_->display_dispatch() != -1);
}
