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
// clang-tidy: The checks below are suppressed for the entire example file.
// These examples interface directly with Vulkan, EGL, Wayland, and OpenGL
// C APIs that fundamentally require pointer arithmetic, non-const globals
// for signal-handler flags, array-to-pointer decay, and reinterpret_cast
// at C API boundaries. Suppressing per-line would add hundreds of NOLINT
// annotations with no increase in safety — the patterns are intentional
// and audited.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,
//             cppcoreguidelines-pro-bounds-pointer-arithmetic,
//             cppcoreguidelines-pro-bounds-array-to-pointer-decay,
//             cppcoreguidelines-pro-bounds-constant-array-index,
//             cppcoreguidelines-pro-type-reinterpret-cast)

#include <algorithm>
#include <array>
#include <atomic>
#include <csignal>
#include <cstdint>

#include <cxxopts.hpp>

#include "logging/logging.h"
#include "waypp/window/xdg_toplevel.h"
#include "waypp/window_manager/window_manager_factory.h"
#include "xdg-output-unstable-v1-client-protocol.h"

struct Configuration {
  int width;
  int height;
  bool fullscreen;
  int maximized;
  bool fullscreen_ratio;
  bool tearing;
};

static constexpr int kResizeMargin = 12;

static std::atomic<bool> running{true};

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
void handle_signal(const int signal) {
  if (signal == SIGINT) {
    running.store(false, std::memory_order_relaxed);
  }
}

static void paint_pixels(void* image,
                         const int padding,
                         const int width,
                         const int height,
                         const uint32_t time) {
  const int half_height = padding + (height - padding * 2) / 2;
  const int half_width = padding + (width - padding * 2) / 2;
  auto* pixel = static_cast<uint32_t*>(image);

  /* squared radii thresholds */
  auto or_ = (half_width < half_height ? half_width : half_height) - 8;
  auto ir = or_ - 32;
  or_ *= or_;
  ir *= ir;

  pixel += padding * width;
  for (auto y = padding; y < height - padding; y++) {
    const int y2 = (y - half_height) * (y - half_height);

    pixel += padding;
    for (auto x = padding; x < width - padding; x++) {
      uint32_t v;

      /* squared distance from a center */
      if (const int r2 = (x - half_width) * (x - half_width) + y2; r2 < ir)
        v = (static_cast<uint32_t>(r2 / 32) + time / 64) * 0x0080401;
      else if (r2 < or_)
        v = (static_cast<uint32_t>(y) + time / 32) * 0x0080401;
      else
        v = (static_cast<uint32_t>(x) + time / 16) * 0x0080401;
      v &= 0x00ffffff;

      /* cross if the compositor uses X from XRGB as alpha */
      if (abs(x - y) > 6 && abs(x + y - height) > 6)
        v |= 0xff000000;

      *pixel++ = v;
    }

    pixel += padding;
  }
}

void draw_frame(void* data, const uint32_t time) {
  const auto window = static_cast<Window*>(data);

  // Flush any pending compositor-driven resize so extents_.window is current
  // before next_buffer() checks dimensions and paint_pixels writes into it.
  window->update_buffer_geometry();

  const auto buffer = window->next_buffer();
  if (!buffer) {
    spdlog::error(
        "[draw_frame] Failed to acquire a buffer — stopping render loop");
    window->stop_frame_callbacks();
    window->close();
    running.store(false, std::memory_order_relaxed);
    return;
  }

  paint_pixels(buffer->get_shm_data(), 20, window->get_width(),
               window->get_height(), time);

  wl_surface_attach(window->get_surface(), buffer->get_wl_buffer(), 0, 0);
  const int damage_w = std::max(0, window->get_width() - 40);
  const int damage_h = std::max(0, window->get_height() - 40);
  if (damage_w > 0 && damage_h > 0) {
    wl_surface_damage(window->get_surface(), 20, 20, damage_w, damage_h);
  }

  buffer->set_busy();
}

static void handle_interface1_add(Registrar* /* data */,
                                  wl_registry* /* registry */,
                                  uint32_t name,
                                  const char* interface,
                                  uint32_t version) {
  spdlog::info("handle_interface1_add: name: {}, interface: {}, version: {}",
               name, interface, version);
}

static void handle_interface1_remove(Registrar* /* data */,
                                     wl_registry* /* registry */,
                                     uint32_t id) {
  spdlog::info("handle_interface1_remove: id: {}", id);
}

static constexpr std::array<Registrar::RegistrarCallback, 1> ext_interfaces{{{
    "weston_capture_v1",
    handle_interface1_add,
    handle_interface1_remove,
}}};

int main(const int argc, char** argv) {
  auto log_init = std::make_unique<Logging>();

  auto display = wl_display_connect(nullptr);
  if (!display) {
    spdlog::critical("Unable to connect to Wayland socket.");
    return EXIT_FAILURE;
  }

  std::signal(SIGINT, handle_signal);

  cxxopts::Options options("simple-shm", "Weston simple-shm example");
  options.add_options()
      // clang-format off
            ("w,width", "Set width", cxxopts::value<int>()->default_value("250"))
            ("h,height", "Set height", cxxopts::value<int>()->default_value("250"))
            ("f,fullscreen", "Run in fullscreen mode")
            ("m,maximized", "Run in maximized mode")
            ("r,fullscreen-ratio", "Use fixed width/height ratio when run in fullscreen mode")
            ("t,tearing", "Enable tearing via the tearing_control protocol");

  // clang-format on
  const auto result = options.parse(argc, argv);

  const Configuration config = {
      .width = result["width"].as<int>(),
      .height = result["height"].as<int>(),
      .fullscreen = result["fullscreen"].as<bool>(),
      .maximized = result["maximized"].as<bool>() ? 1 : 0,
      .fullscreen_ratio = result["fullscreen-ratio"].as<bool>(),
      .tearing = result["tearing"].as<bool>(),
  };

  auto [wm_base, wm_type] = WindowManagerFactory::create(
      display, false, ext_interfaces.size(), ext_interfaces.data());
  if (wm_type == WindowManagerType::kIvi) {
    spdlog::critical("simple-ext-protocol: IVI shell not supported");
    wl_display_disconnect(display);
    return EXIT_FAILURE;
  }
  auto wm = std::static_pointer_cast<XdgWindowManager>(wm_base);
  spdlog::info("XDG Window Manager Version: {}", wm->get_version());
  auto top_level = wm->create_top_level(
      "simple-ext-protocol", "jwinarske.waypp.simple_ext_protocol",
      config.width, config.height, kResizeMargin, 2, WL_SHM_FORMAT_XRGB8888,
      config.fullscreen, config.maximized, config.fullscreen_ratio,
      config.tearing, draw_frame);
  spdlog::info("XDG Window Version: {}", top_level->get_version());

  /// paint padding
  top_level->set_surface_damage(0, 0, config.width, config.height);
  top_level->update_buffer_geometry();
  top_level->start_frame_callbacks();

  while (running.load(std::memory_order_acquire) && top_level->is_valid() &&
         wm->display_dispatch() != -1) {
  }

  top_level.reset();
  wm.reset();

  wl_display_flush(display);
  wl_display_disconnect(display);

  return EXIT_SUCCESS;
}
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,
//           cppcoreguidelines-pro-bounds-pointer-arithmetic,
//           cppcoreguidelines-pro-bounds-array-to-pointer-decay,
//           cppcoreguidelines-pro-bounds-constant-array-index,
//           cppcoreguidelines-pro-type-reinterpret-cast)
