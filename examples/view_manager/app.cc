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

#include "view_manager_wayland.h"

class ViewManagerWayland;

App::App(const ViewManager::Configuration& config) {
  logging_ = std::make_unique<Logging>();

  view_manager_wayland_ = std::make_unique<ViewManagerWayland>(config);

  view_manager_wayland_->create_view(
      kAppTitle, kAppId, config.width, config.height, config.fullscreen,
      config.maximized, config.fullscreen_ratio, config.tearing);
}

App::~App() = default;

bool App::run() const {
  return view_manager_wayland_->poll_events();
}

void App::toggle_fullscreen() const {
  view_manager_wayland_->toggle_fullscreen();
}
