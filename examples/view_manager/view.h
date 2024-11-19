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

#ifndef EXAMPLES_VIEW_MANAGER_VIEW_H_
#define EXAMPLES_VIEW_MANAGER_VIEW_H_

#include <cstdint>

#include "view_manager.h"

class View {
 public:
  View() = default;

  virtual ~View() = default;

  virtual void close() = 0;

  virtual bool is_valid() = 0;

  virtual void toggle_fullscreen() = 0;

  virtual uint32_t check_edge_resize(std::pair<double, double> xy) = 0;

  virtual void resize(struct wl_seat* seat,
                      uint32_t serial,
                      uint32_t edges) = 0;

  // Disallow copy and assign.
  View(const View&) = delete;

  View& operator=(const View&) = delete;

 private:
};

#endif  // EXAMPLES_VIEW_MANAGER_VIEW_H_