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

#ifndef EXAMPLES_VIEW_MANAGER_VIEW_MANAGER_H_
#define EXAMPLES_VIEW_MANAGER_VIEW_MANAGER_H_

#include <list>
#include <vector>
#include <memory>

#include "view.h"

class View;

class ViewManager {
public:
    struct Configuration {
        int width;
        int height;
        bool disable_cursor;
        bool fullscreen;
        bool maximized;
        bool fullscreen_ratio;
        bool tearing;
    };

    ViewManager() = default;

    ~ViewManager() = default;

    virtual bool poll_events() = 0;

    virtual uint32_t
    create_view(const char *app_title, const char *app_id, int width, int height, bool fullscreen, bool maximized,
                bool fullscreen_ratio, bool tearing) = 0;

    virtual void quit() = 0;

    virtual void toggle_fullscreen() = 0;

    // Disallow copy and assign.
    ViewManager(const ViewManager &) = delete;

    ViewManager &operator=(const ViewManager &) = delete;

private:
    std::list<std::unique_ptr<View>> views_;
};

#endif //EXAMPLES_VIEW_MANAGER_VIEW_MANAGER_H_