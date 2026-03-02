/*
 * Copyright © 2026 Joel Winarske
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

#include <atomic>
#include <csignal>
#include <random>
#include <stdexcept>

#include <cxxopts.hpp>

#include "logging/logging.h"

#include "waypp/window/ivi_surface.h"
#include "waypp/window_manager/ivi_window_manager.h"
#include "waypp/window_manager/ivi_wm.h"
#include "waypp/window_manager/window_manager_factory.h"

struct Configuration {
  uint32_t ivi_id;
  int width;
  int height;
  bool disable_cursor;
};

static std::atomic<bool> gRunning{true};

static std::vector<std::string> gCursors = Pointer::get_available_cursors();

/**
 * @brief SIGINT handler — signals the run loop to exit cleanly.
 */
void handle_signal(const int signal) {
  if (signal == SIGINT) {
    gRunning.store(false, std::memory_order_relaxed);
  }
}

// ── Pixel painter (identical to simple-shm / agl-simple-shm) ──────────────

static void paint_pixels(void* image,
                         const int padding,
                         const int width,
                         const int height,
                         const uint32_t time) {
  auto pixel = static_cast<uint32_t*>(image);
  const int half_h = padding + (height - padding * 2) / 2;
  const int half_w = padding + (width - padding * 2) / 2;

  auto or_ = (half_w < half_h ? half_w : half_h) - 8;
  auto ir = or_ - 32;
  or_ *= or_;
  ir *= ir;

  pixel += padding * width;
  for (auto y = padding; y < height - padding; y++) {
    const auto y2 = (y - half_h) * (y - half_h);

    pixel += padding;
    for (auto x = padding; x < width - padding; x++) {
      uint32_t v;

      if (const int r2 = (x - half_w) * (x - half_w) + y2; r2 < ir)
        v = (static_cast<uint32_t>(r2 / 32) + time / 64) * 0x0080401;
      else if (r2 < or_)
        v = (static_cast<uint32_t>(y) + time / 32) * 0x0080401;
      else
        v = (static_cast<uint32_t>(x) + time / 16) * 0x0080401;
      v &= 0x00ffffff;

      if (abs(x - y) > 6 && abs(x + y - height) > 6)
        v |= 0xff000000;

      *pixel++ = v;
    }

    pixel += padding;
  }
}

// ── Frame callback ─────────────────────────────────────────────────────────

void draw_frame(void* data, const uint32_t time) {
  const auto window = static_cast<Window*>(data);

  // Apply any pending geometry update (compositor configure → resize).
  window->update_buffer_geometry();

  const auto buffer = window->next_buffer();
  if (!buffer) {
    spdlog::error(
        "[draw_frame] Failed to acquire a buffer — stopping render loop");
    window->stop_frame_callbacks();
    window->close();
    gRunning.store(false, std::memory_order_relaxed);
    return;
  }

  paint_pixels(buffer->get_shm_data(), 20, window->get_width(),
               window->get_height(), time);

  wl_surface_attach(window->get_surface(), buffer->get_wl_buffer(), 0, 0);

  const int damage_width = std::max(0, window->get_width() - 40);
  const int damage_height = std::max(0, window->get_height() - 40);
  if (damage_width > 0 && damage_height > 0) {
    wl_surface_damage(window->get_surface(), 20, 20, damage_width,
                      damage_height);
  }

  buffer->set_busy();
}

// ── Application class ──────────────────────────────────────────────────────

class App final : public PointerObserver,
                  public KeyboardObserver,
                  public SeatObserver {
 public:
  explicit App(const Configuration& config) : gen_(rd_()) {
    logging_ = std::make_unique<Logging>();
    wl_display_ = wl_display_connect(nullptr);
    if (!wl_display_) {
      throw std::runtime_error("Unable to connect to Wayland display socket");
    }

    auto [wm, wm_type] =
        WindowManagerFactory::create(wl_display_, config.disable_cursor);
    if (wm_type != WindowManagerType::kIvi) {
      throw std::runtime_error("ivi-simple-shm: ivi_application not present");
    }
    wm_ = std::static_pointer_cast<IviWindowManager>(wm);

    if (wm_->get_seat().has_value()) {
      seat_ = wm_->get_seat().value();
      seat_->register_observer(this);
    }

    // IviWm is optional — only available on compositors that advertise ivi_wm.
    if (auto* raw_wm = wm_->get_ivi_wm()) {
      ivi_wm_ = std::make_unique<IviWm>(raw_wm);
    }

    surface_ =
        wm_->create_ivi_surface(config.ivi_id, config.width, config.height, 2,
                                WL_SHM_FORMAT_XRGB8888, draw_frame);

    spdlog::info("IVI surface created: ivi_id={} size={}x{}", config.ivi_id,
                 config.width, config.height);

    // Damage the full surface area to ensure the first frame is sent.
    wl_surface_damage(surface_->get_surface(), 0, 0, config.width,
                      config.height);

    surface_->start_frame_callbacks();
  }

  ~App() override {
    surface_.reset();
    ivi_wm_.reset();
    wm_.reset();

    if (wl_display_) {
      wl_display_flush(wl_display_);
      wl_display_disconnect(wl_display_);
    }
  }

  [[nodiscard]] bool run() const {
    return (surface_->is_valid() && wm_->display_dispatch() != -1);
  }

  // ── SeatObserver ────────────────────────────────────────────────────────

  void notify_seat_capabilities(Seat* seat,
                                wl_seat* /* seat */,
                                uint32_t /* caps */) override {
    if (seat) {
      if (seat->get_keyboard().has_value()) {
        seat->get_keyboard().value()->register_observer(this);
      }
      if (seat->get_pointer().has_value()) {
        seat->get_pointer().value()->register_observer(this);
      }
    }
  }

  void notify_seat_name(Seat* /* seat */,
                        wl_seat* /* seat */,
                        const char* name) override {
    spdlog::info("Seat: {}", name);
  }

  // ── KeyboardObserver ────────────────────────────────────────────────────

  void notify_keyboard_enter(Keyboard* /* keyboard */,
                             wl_keyboard* /* wl_keyboard */,
                             uint32_t serial,
                             wl_surface* surface,
                             wl_array* /* keys */) override {
    spdlog::info("Keyboard Enter: serial: {}, surface: {}", serial,
                 fmt::ptr(surface));
  }

  void notify_keyboard_leave(Keyboard* /* keyboard */,
                             wl_keyboard* /* wl_keyboard */,
                             uint32_t serial,
                             wl_surface* surface) override {
    spdlog::info("Keyboard Leave: serial: {}, surface: {}", serial,
                 fmt::ptr(surface));
  }

  void notify_keyboard_keymap(Keyboard* /* keyboard */,
                              wl_keyboard* /* wl_keyboard */,
                              uint32_t format,
                              int32_t fd,
                              uint32_t size) override {
    spdlog::info("Keymap: format: {}, fd: {}, size: {}", format, fd, size);
  }

  void notify_keyboard_xkb_v1_key(
      Keyboard* /* keyboard */,
      wl_keyboard* /* wl_keyboard */,
      uint32_t serial,
      uint32_t time,
      uint32_t xkb_scancode,
      bool key_repeats,
      const uint32_t state,
      int xdg_key_symbol_count,
      const xkb_keysym_t* xdg_key_symbols) override {
    spdlog::info(
        "Key: serial: {}, time: {}, xkb_scancode: 0x{:X}, key_repeats: {}, "
        "state: {}, xdg_keysym_count: {}, syms_out[0]: 0x{:X}",
        serial, time, xkb_scancode, key_repeats,
        state == KeyState::KEY_STATE_PRESS ? "press" : "release",
        xdg_key_symbol_count, xdg_key_symbols[0]);
  }

  // ── PointerObserver ─────────────────────────────────────────────────────

  void notify_pointer_enter(Pointer* pointer,
                            wl_pointer* /* pointer */,
                            uint32_t serial,
                            wl_surface* surface,
                            double sx,
                            double sy) override {
    spdlog::info("Pointer Enter: serial: {}, surface: {}, x: {}, y: {}", serial,
                 fmt::ptr(surface), sx, sy);

    if (gCursors.size() > 1) {
      std::uniform_int_distribution<size_t> distribution(0,
                                                         gCursors.size() - 1);
      pointer->set_cursor(serial, gCursors[distribution(gen_)].c_str());
    } else {
      pointer->set_cursor(serial, "crosshair");
    }
  }

  void notify_pointer_leave(Pointer* /* pointer */,
                            wl_pointer* /* pointer */,
                            uint32_t serial,
                            wl_surface* surface) override {
    spdlog::info("Pointer Leave: serial: {}, surface: {}", serial,
                 fmt::ptr(surface));
  }

  void notify_pointer_motion(Pointer* /* pointer */,
                             wl_pointer* /* pointer */,
                             uint32_t time,
                             double sx,
                             double sy) override {
    spdlog::info("Pointer Motion: time: {}, x: {}, y: {}", time, sx, sy);
  }

  void notify_pointer_button(Pointer* /* pointer */,
                             wl_pointer* /* pointer */,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t button,
                             uint32_t state) override {
    spdlog::info("Pointer Button: serial: {}, time: {}, button: {}, state: {}",
                 serial, time, button, state);
  }

  void notify_pointer_axis(Pointer* /* pointer */,
                           wl_pointer* /* pointer */,
                           uint32_t time,
                           uint32_t axis,
                           double value) override {
    spdlog::info("Pointer Axis: time: {}, axis: {}, value: {}", time, axis,
                 value);
  }

  void notify_pointer_frame(Pointer* /* pointer */,
                            wl_pointer* /* pointer */) override {
    spdlog::info("Pointer Frame");
  }

  void notify_pointer_axis_source(Pointer* /* pointer */,
                                  wl_pointer* /* pointer */,
                                  uint32_t axis_source) override {
    spdlog::info("Pointer Axis Source: axis_source: {}", axis_source);
  }

  void notify_pointer_axis_stop(Pointer* /* pointer */,
                                wl_pointer* /* pointer */,
                                uint32_t /* time */,
                                uint32_t axis) override {
    spdlog::info("Pointer Axis Stop: axis: {}", axis);
  }

  void notify_pointer_axis_discrete(Pointer* /* pointer */,
                                    wl_pointer* /* pointer */,
                                    uint32_t axis,
                                    int32_t discrete) override {
    spdlog::info("Pointer Axis Discrete: axis: {}, discrete: {}", axis,
                 discrete);
  }

 private:
  std::random_device rd_;
  std::mt19937 gen_;
  wl_display* wl_display_{};
  std::unique_ptr<Logging> logging_{};
  std::shared_ptr<IviWindowManager> wm_;
  std::unique_ptr<IviWm> ivi_wm_;
  Seat* seat_{};
  std::shared_ptr<IviSurface> surface_;
};

// ── Entry point ────────────────────────────────────────────────────────────

int main(const int argc, char** argv) {
  std::signal(SIGINT, handle_signal);

  cxxopts::Options options("ivi-simple-shm",
                           "waypp IVI-shell SHM rendering example");
  options.add_options()
      // clang-format off
      ("i,ivi-id",        "IVI surface ID",  cxxopts::value<uint32_t>()->default_value("9000"))
      ("w,width",         "Surface width",   cxxopts::value<int>()->default_value("800"))
      ("h,height",        "Surface height",  cxxopts::value<int>()->default_value("480"))
      ("c,disable-cursor","Disable cursor");
  // clang-format on

  const auto result = options.parse(argc, argv);

  try {
    const App app({
        .ivi_id = result["ivi-id"].as<uint32_t>(),
        .width = result["width"].as<int>(),
        .height = result["height"].as<int>(),
        .disable_cursor = result["disable-cursor"].as<bool>(),
    });

    while (gRunning.load(std::memory_order_acquire) && app.run()) {
    }
  } catch (const std::runtime_error& e) {
    spdlog::critical("Fatal error: {}", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
