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

    // Pass this as user_data — draw_frame receives ViewWayland* as data.
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
                                           uint32_t* buffer,
                                           const std::size_t buffer_size) {
  // ── Sanity guards ────────────────────────────────────────────────────────
  // grid_size == 0 would cause division-by-zero below.
  if (grid_size == 0) {
    LOG_ERROR("[create_random_color_grid] grid_size must be > 0");
    return;
  }

  // Compute the total pixel count in size_t to avoid uint32_t overflow for
  // large compositor-provided dimensions (e.g. width=65536 would overflow).
  // Each pixel is one uint32_t (4 bytes).
  const auto w = static_cast<std::size_t>(width);
  const auto h = static_cast<std::size_t>(height);

  if (const std::size_t required_bytes = w * h * sizeof(uint32_t);
      required_bytes == 0 || required_bytes > buffer_size) {
    LOG_ERROR(
        "[create_random_color_grid] buffer too small: need {} bytes, "
        "have {} bytes (width={}, height={})",
        required_bytes, buffer_size, width, height);
    return;
  }

  // ── Grid setup ───────────────────────────────────────────────────────────
  const std::size_t grid_w = w / static_cast<std::size_t>(grid_size);
  const std::size_t grid_h = h / static_cast<std::size_t>(grid_size);

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
  for (std::size_t i = 0; i < static_cast<std::size_t>(grid_size); ++i) {
    for (std::size_t j = 0; j < static_cast<std::size_t>(grid_size); ++j) {
      // Corner colors for this cell
      const uint32_t c00 = color_grid[i][j];
      const uint32_t c01 = color_grid[i][j + 1];
      const uint32_t c10 = color_grid[i + 1][j];
      const uint32_t c11 = color_grid[i + 1][j + 1];

      // Iterate over each pixel within the cell
      for (std::size_t x = 0; x < grid_w; ++x) {
        for (std::size_t y = 0; y < grid_h; ++y) {
          const auto tx = static_cast<double>(x) / static_cast<double>(grid_w);
          const auto ty = static_cast<double>(y) / static_cast<double>(grid_h);

          // Bilinearly interpolate colors for this pixel
          const auto r = (1.0 - tx) * (1.0 - ty) * (c00 >> 16 & 0xFF) +
                         tx * (1.0 - ty) * (c10 >> 16 & 0xFF) +
                         (1.0 - tx) * ty * (c01 >> 16 & 0xFF) +
                         tx * ty * (c11 >> 16 & 0xFF);
          const auto g = (1.0 - tx) * (1.0 - ty) * (c00 >> 8 & 0xFF) +
                         tx * (1.0 - ty) * (c10 >> 8 & 0xFF) +
                         (1.0 - tx) * ty * (c01 >> 8 & 0xFF) +
                         tx * ty * (c11 >> 8 & 0xFF);
          const auto b = (1.0 - tx) * (1.0 - ty) * (c00 & 0xFF) +
                         tx * (1.0 - ty) * (c10 & 0xFF) +
                         (1.0 - tx) * ty * (c01 & 0xFF) +
                         tx * ty * (c11 & 0xFF);

          // All index arithmetic in size_t — no uint32_t overflow possible.
          // Maximum index: (grid_size-1)*grid_h + (grid_h-1)) * w
          //              + (grid_size-1)*grid_w + (grid_w-1)
          //              < h * w  == checked above against buffer_size.
          const std::size_t idx =
              (i * grid_h + y) * w + (j * grid_w + x);
          buffer[idx] = (static_cast<uint32_t>(r) << 16) |
                        (static_cast<uint32_t>(g) << 8) |
                         static_cast<uint32_t>(b);
        }
      }
    }
  }
}

void ViewWayland::draw_frame(void* data, const uint32_t /* time */) {
  const auto view = static_cast<ViewWayland*>(data);
  const auto window = static_cast<Window*>(view->toplevel_.get());

  // Flush any pending geometry update (compositor configure → resize) so that
  // window->get_width()/get_height() and the buffer dimensions are current
  // before we attempt to acquire a buffer.
  window->update_buffer_geometry();

  const auto buffer = window->next_buffer();
  if (!buffer) {
    spdlog::error("Failed to acquire a buffer");
    // Do not call exit(): we are inside a wl_surface_frame callback.
    // Halt the frame-callback chain and signal the run loop to exit cleanly.
    window->stop_frame_callbacks();
    window->close();
    return;
  }

  view->create_random_color_grid(
      static_cast<uint32_t>(window->get_width()),
      static_cast<uint32_t>(window->get_height()), 8,
      static_cast<uint32_t*>(buffer->get_shm_data()),
      static_cast<std::size_t>(buffer->get_size()));

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
