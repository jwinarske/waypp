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

#ifndef EXAMPLES_VIEW_MANAGER_APP_H_
#define EXAMPLES_VIEW_MANAGER_APP_H_

#include "logging/logging.h"

#include "view_manager.h"
#include "view_manager_wayland.h"

class ViewManagerWayland;

class App {
public:
    static constexpr char kAppTitle[] = "view-manager";
    static constexpr char kAppId[] = "org.waypp.view-manager";

    explicit App(const ViewManager::Configuration &config);

    ~App();

    bool run();

    void toggle_fullscreen();

private:
    std::unique_ptr<Logging> logging_;
    std::unique_ptr<ViewManagerWayland> view_manager_wayland_;
};

#endif