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

#ifndef EXAMPLES_VIEW_MANAGER_VIEW_WAYLAND_H_
#define EXAMPLES_VIEW_MANAGER_VIEW_WAYLAND_H_

#include "view.h"
#include "view_manager_wayland.h"
#include "waypp/window/xdg_toplevel.h"

#include <cstddef>
#include <random>

class ViewWayland final : public View {
 public:
  ViewWayland(std::shared_ptr<XdgWindowManager> xdg_window_manager,
              const char* app_title,
              const char* app_id,
              int width,
              int height,
              bool fullscreen,
              bool maximized,
              bool fullscreen_ratio,
              bool tearing,
              bool toplevel = true);

  ~ViewWayland() override;

  void close() override;

  bool is_valid() override;

  void toggle_fullscreen() override;

  uint32_t check_edge_resize(std::pair<double, double> xy) override;

  void resize(struct wl_seat* seat, uint32_t serial, uint32_t edges) override;

  // Disallow copy and assign.
  ViewWayland(const ViewWayland&) = delete;

  ViewWayland& operator=(const ViewWayland&) = delete;

 private:
  static constexpr int kResizeMargin = 12;
  std::shared_ptr<XdgTopLevel> toplevel_{};
  std::shared_ptr<XdgWindowManager> xdg_wm_;

  std::random_device rd_;
  std::mt19937 gen_;
  std::uniform_int_distribution<uint32_t> distribution_;

  void create_random_color_grid(uint32_t width,
                                uint32_t height,
                                uint32_t grid_size,
                                uint32_t* buffer,
                                std::size_t buffer_size);

  static void draw_frame(void* data, std::uint32_t time);
};

#endif  // EXAMPLES_VIEW_MANAGER_VIEW_WAYLAND_H_