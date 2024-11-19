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

#include "view_wayland.h"
#include "logging/logging.h"

ViewWayland::ViewWayland(std::shared_ptr<XdgWindowManager> xdg_window_manager,
                         const char* app_title,
                         const char* app_id,
                         const int width,
                         const int height,
                         const bool fullscreen,
                         const bool maximized,
                         const bool fullscreen_ratio,
                         const bool tearing,
                         const bool toplevel)
    : gen_(rd_()), distribution_(0, 255) {
  xdg_wm_ = std::move(xdg_window_manager);

  if (toplevel) {
    toplevel_ = xdg_wm_->create_top_level(
        app_title, app_id, width, height, kResizeMargin, 2,
        WL_SHM_FORMAT_XRGB8888, fullscreen, maximized, fullscreen_ratio,
        tearing, draw_frame);
    spdlog::debug("XDG Window Version: {}", toplevel_->get_version());

    toplevel_->start_frame_callbacks(this);
  }
}

ViewWayland::~ViewWayland() = default;

void ViewWayland::close() {
  if (toplevel_) {
    toplevel_->close();
  }
}

bool ViewWayland::is_valid() {
  return toplevel_->is_valid();
}

void ViewWayland::toggle_fullscreen() {
  if (toplevel_) {
    toplevel_->set_fullscreen();
  }
}

void ViewWayland::create_random_color_grid(const uint32_t width,
                                           const uint32_t height,
                                           const uint32_t grid_size,
                                           uint32_t* buffer) {
  const uint32_t grid_width = width / grid_size;
  const uint32_t grid_height = height / grid_size;

  // Grid to hold the corner colors
  std::vector<std::vector<uint32_t>> color_grid(
      grid_size + 1, std::vector<uint32_t>(grid_size + 1));

  // Generate a random color for each cell in the grid
  for (uint32_t i = 0; i <= grid_size; ++i) {
    for (uint32_t j = 0; j <= grid_size; ++j) {
      color_grid[i][j] = (distribution_(gen_) << 16) |
                         (distribution_(gen_) << 8) | distribution_(gen_);
    }
  }

  // Iterate over each cell in the grid
  for (uint32_t i = 0; i < grid_size; ++i) {
    for (uint32_t j = 0; j < grid_size; ++j) {
      // Corner colors for this cell
      const uint32_t c00 = color_grid[i][j];
      const uint32_t c01 = color_grid[i][j + 1];
      const uint32_t c10 = color_grid[i + 1][j];
      const uint32_t c11 = color_grid[i + 1][j + 1];

      // Iterate over each pixel within the cell
      for (uint32_t x = 0; x < grid_width; ++x) {
        for (uint32_t y = 0; y < grid_height; ++y) {
          const auto tx = x / grid_width;
          const auto ty = y / grid_height;

          // Bilinearly interpolate colors for this pixel
          const auto r = (1 - tx) * (1 - ty) * (c00 >> 16 & 0xFF) +
                         tx * (1 - ty) * (c10 >> 16 & 0xFF) +
                         (1 - tx) * ty * (c01 >> 16 & 0xFF) +
                         tx * ty * (c11 >> 16 & 0xFF);
          const auto g = (1 - tx) * (1 - ty) * (c00 >> 8 & 0xFF) +
                         tx * (1 - ty) * (c10 >> 8 & 0xFF) +
                         (1 - tx) * ty * (c01 >> 8 & 0xFF) +
                         tx * ty * (c11 >> 8 & 0xFF);
          const auto b = (1 - tx) * (1 - ty) * (c00 & 0xFF) +
                         tx * (1 - ty) * (c10 & 0xFF) +
                         (1 - tx) * ty * (c01 & 0xFF) + tx * ty * (c11 & 0xFF);

          // Set pixel color in the final buffer
          buffer[((i * grid_height + y) * width) + (j * grid_width + x)] =
              (r << 16) | (g << 8) | b;
        }
      }
    }
  }
}

void ViewWayland::draw_frame(void* data, const uint32_t /* time */) {
  const auto window = static_cast<Window*>(data);
  const auto view = static_cast<ViewWayland*>(window->get_user_data());

  const auto buffer = window->next_buffer();
  if (!buffer) {
    spdlog::error("Failed to acquire a buffer");
    exit(EXIT_FAILURE);
  }

  view->create_random_color_grid(
      static_cast<uint32_t>(window->get_width()),
      static_cast<uint32_t>(window->get_height()), 8,
      static_cast<uint32_t*>(buffer->get_shm_data()));

  wl_surface_attach(window->get_surface(), buffer->get_wl_buffer(), 0, 0);
  wl_surface_damage(window->get_surface(), 0, 0, window->get_width(),
                    window->get_height());

  buffer->set_busy();
}

uint32_t ViewWayland::check_edge_resize(const std::pair<double, double> xy) {
  return toplevel_->check_edge_resize(xy);
}

void ViewWayland::resize(struct wl_seat* seat,
                         uint32_t serial,
                         uint32_t edges) {
  toplevel_->resize(seat, serial, edges);
}
