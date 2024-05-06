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
#include "window/xdg_toplevel.h"
#include "vk_backend.h"

class App {
public:

    static constexpr char kAppTitle[] = "Shadertoy";
    static constexpr char kAppId[] = "org.waypp.vk-shadertoy";

    struct Configuration {
        int width;
        int height;
        bool debug_enable;
        bool disable_cursor;
        bool fullscreen;
        bool maximized;
        bool fullscreen_ratio;
        bool tearing;
    };

    explicit App(const Configuration &config);

    ~App();

    bool run();

private:
    struct wl_display *display_;
    std::unique_ptr<Logging> logging_;
    std::unique_ptr<Handlers> handlers_;
    std::unique_ptr<XdgWindowManager> wm_;
    std::unique_ptr<VulkanBackend> backend_;
    XdgTopLevel *toplevel_;

    static void draw_frame(void *data, uint32_t time);
};
