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

/**
 * @file simple-csd.cc
 *
 * Demonstrates the waypp client-side decorations (CSD) — title bar, resize
 * handles, and window buttons — rendered natively via CsdShmPlugin without
 * any additional library dependency.
 *
 * Compared to simple-shm:
 *  - create_top_level() is called with enable_csd=true (unless --no-csd).
 *  - The App's PointerObserver does NOT call check_edge_resize() or
 *    toplevel_->resize() — CsdFrame owns all decoration pointer events.
 *  - A --title flag lets you set the title bar text at start-up.
 *  - A --no-csd flag disables decorations for direct comparison.
 */

#include <atomic>
#include <csignal>
#include <stdexcept>

#include <linux/input.h>
#include <cxxopts.hpp>

#include "logging/logging.h"
#include "waypp/window/xdg_toplevel.h"
#include "waypp/window_manager/window_manager_factory.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

struct Configuration {
  int width;
  int height;
  bool disable_cursor;
  bool fullscreen;
  bool maximized;
  bool fullscreen_ratio;
  bool tearing;
  bool no_csd;
  std::string title;
};

static constexpr int kResizeMargin = 12;

static std::atomic<bool> gRunning{true};

// ---------------------------------------------------------------------------
// Signal handler
// ---------------------------------------------------------------------------

void handle_signal(const int signal) {
  if (signal == SIGINT) {
    gRunning.store(false, std::memory_order_relaxed);
  }
}

// ---------------------------------------------------------------------------
// Frame callback — identical to simple-shm
// ---------------------------------------------------------------------------

static constexpr int kPaintPadding = 20;

static void paint_pixels(void* image,
                         const int width,
                         const int height,
                         const uint32_t time) {
  auto* pixel = static_cast<uint32_t*>(image);
  const int half_h = kPaintPadding + (height - kPaintPadding * 2) / 2;
  const int half_w = kPaintPadding + (width - kPaintPadding * 2) / 2;

  auto or_ = (half_w < half_h ? half_w : half_h) - 8;
  auto ir = or_ - 32;
  or_ *= or_;
  ir *= ir;

  pixel += kPaintPadding * width;
  for (auto y = kPaintPadding; y < height - kPaintPadding; y++) {
    const int y2 = (y - half_h) * (y - half_h);
    pixel += kPaintPadding;
    for (auto x = kPaintPadding; x < width - kPaintPadding; x++) {
      uint32_t v;
      if (const int r2 = (x - half_w) * (x - half_w) + y2; r2 < ir)
        v = (static_cast<uint32_t>(r2 / 32) + time / 64) * 0x0080401u;
      else if (r2 < or_)
        v = (static_cast<uint32_t>(y) + time / 32) * 0x0080401u;
      else
        v = (static_cast<uint32_t>(x) + time / 16) * 0x0080401u;
      v &= 0x00FFFFFFu;
      if (abs(x - y) > 6 && abs(x + y - height) > 6)
        v |= 0xFF000000u;
      *pixel++ = v;
    }
    pixel += kPaintPadding;
  }
}

void draw_frame(void* data, const uint32_t time) {
  const auto window = static_cast<Window*>(data);

  window->update_buffer_geometry();

  const auto buffer = window->next_buffer();
  if (!buffer) {
    spdlog::error("[draw_frame] Failed to acquire buffer — stopping");
    window->stop_frame_callbacks();
    window->close();
    gRunning.store(false, std::memory_order_relaxed);
    return;
  }

  paint_pixels(buffer->get_shm_data(), window->get_width(),
               window->get_height(), time);

  wl_surface_attach(window->get_surface(), buffer->get_wl_buffer(), 0, 0);

  const int dw = std::max(0, window->get_width() - 40);
  const int dh = std::max(0, window->get_height() - 40);
  if (dw > 0 && dh > 0) {
    wl_surface_damage(window->get_surface(), 20, 20, dw, dh);
  }

  buffer->set_busy();
}

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------

class App final : public PointerObserver,
                  public KeyboardObserver,
                  public SeatObserver {
 public:
  explicit App(const Configuration& config) {
    logging_ = std::make_unique<Logging>();
    wl_display_ = wl_display_connect(nullptr);
    if (!wl_display_) {
      throw std::runtime_error("Unable to connect to Wayland display socket");
    }

    auto [wm, wm_type] =
        WindowManagerFactory::create(wl_display_, config.disable_cursor);
    wm_ = std::move(wm);
    if (wm_->get_seat().has_value()) {
      seat_ = wm_->get_seat().value();
      seat_->register_observer(this);
    }

    if (wm_type == WindowManagerType::kXdg) {
      const auto xdg_wm = std::static_pointer_cast<XdgWindowManager>(wm_);
      spdlog::info("XDG Window Manager Version: {}", xdg_wm->get_version());

      const bool enable_csd = !config.no_csd;

      toplevel_ = xdg_wm->create_top_level(
          config.title.c_str(),
          "org.freedesktop.gitlab.jwinarske.waypp.simple_csd", config.width,
          config.height, kResizeMargin, 2, WL_SHM_FORMAT_XRGB8888,
          config.fullscreen, config.maximized, config.fullscreen_ratio,
          config.tearing, draw_frame,
          /*egl_config=*/nullptr, enable_csd);

      spdlog::info("XDG Window Version: {}", toplevel_->get_version());
    } else {
      throw std::runtime_error("simple-csd: IVI shell is not supported");
    }

    toplevel_->set_surface_damage(0, 0, config.width, config.height);
    toplevel_->start_frame_callbacks();
  }

  ~App() override {
    toplevel_.reset();
    wm_.reset();

    if (wl_display_) {
      wl_display_flush(wl_display_);
      wl_display_disconnect(wl_display_);
    }
  }

  [[nodiscard]] bool run() const {
    return toplevel_->is_valid() && wm_->display_dispatch() != -1;
  }

  // -------------------------------------------------------------------------
  // SeatObserver
  // -------------------------------------------------------------------------

  void notify_seat_capabilities(Seat* seat,
                                wl_seat* /* wl_seat */,
                                uint32_t /* caps */) override {
    if (!seat)
      return;
    if (seat->get_keyboard().has_value()) {
      seat->get_keyboard().value()->register_observer(this);
    }
    if (seat->get_pointer().has_value()) {
      seat->get_pointer().value()->register_observer(this);
    }
  }

  void notify_seat_name(Seat* /* seat */,
                        wl_seat* /* wl_seat */,
                        const char* name) override {
    spdlog::info("Seat: {}", name);
  }

  // -------------------------------------------------------------------------
  // KeyboardObserver
  // -------------------------------------------------------------------------

  void notify_keyboard_enter(Keyboard* /* keyboard */,
                             wl_keyboard* /* wl_keyboard */,
                             uint32_t serial,
                             wl_surface* surface,
                             wl_array* /* keys */) override {
    spdlog::info("Keyboard Enter: serial={} surface={}", serial,
                 fmt::ptr(surface));
  }

  void notify_keyboard_leave(Keyboard* /* keyboard */,
                             wl_keyboard* /* wl_keyboard */,
                             uint32_t serial,
                             wl_surface* surface) override {
    spdlog::info("Keyboard Leave: serial={} surface={}", serial,
                 fmt::ptr(surface));
  }

  void notify_keyboard_keymap(Keyboard* /* keyboard */,
                              wl_keyboard* /* wl_keyboard */,
                              uint32_t format,
                              int32_t fd,
                              uint32_t size) override {
    spdlog::info("Keymap: format={} fd={} size={}", format, fd, size);
  }

  void notify_keyboard_xkb_v1_key(
      Keyboard* /* keyboard */,
      wl_keyboard* /* wl_keyboard */,
      uint32_t serial,
      uint32_t time,
      uint32_t xkb_scancode,
      bool key_repeats,
      uint32_t state,
      int xdg_key_symbol_count,
      const xkb_keysym_t* xdg_key_symbols) override {
    spdlog::info(
        "Key: serial={} time={} scancode=0x{:X} repeats={} state={} "
        "nsyms={} sym[0]=0x{:X}",
        serial, time, xkb_scancode, key_repeats,
        state == KeyState::KEY_STATE_PRESS ? "press" : "release",
        xdg_key_symbol_count, xdg_key_symbols[0]);
  }

  // -------------------------------------------------------------------------
  // PointerObserver
  //
  // CsdFrame owns all pointer events that land on decoration sub-surfaces.
  // The App's pointer notifications only fire for events over the content
  // area (when CSD is active) or for all surfaces (when --no-csd is set).
  // Do NOT call check_edge_resize() here when CSD is active — CsdFrame
  // already handles interactive resize via its own PointerObserver.
  // -------------------------------------------------------------------------

  void notify_pointer_enter(Pointer* pointer,
                            wl_pointer* /* wl_pointer */,
                            uint32_t serial,
                            wl_surface* surface,
                            double sx,
                            double sy) override {
    spdlog::info("Pointer Enter: serial={} surface={} x={} y={}", serial,
                 fmt::ptr(surface), sx, sy);
    pointer->set_cursor(serial, "left_ptr");
  }

  void notify_pointer_leave(Pointer* /* pointer */,
                            wl_pointer* /* wl_pointer */,
                            uint32_t serial,
                            wl_surface* surface) override {
    spdlog::info("Pointer Leave: serial={} surface={}", serial,
                 fmt::ptr(surface));
  }

  void notify_pointer_motion(Pointer* /* pointer */,
                             wl_pointer* /* wl_pointer */,
                             uint32_t time,
                             double sx,
                             double sy) override {
    spdlog::debug("Pointer Motion: time={} x={} y={}", time, sx, sy);
  }

  void notify_pointer_button(Pointer* /* pointer */,
                             wl_pointer* /* wl_pointer */,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t button,
                             uint32_t state) override {
    spdlog::info("Pointer Button: serial={} time={} button={} state={}", serial,
                 time, button, state);
    // No resize/move handling here — CsdFrame owns that when CSD is active.
    // With --no-csd there are no decorations, so no interactive resize either.
  }

  void notify_pointer_axis(Pointer* /* pointer */,
                           wl_pointer* /* wl_pointer */,
                           uint32_t time,
                           uint32_t axis,
                           double value) override {
    spdlog::debug("Pointer Axis: time={} axis={} value={}", time, axis, value);
  }

  void notify_pointer_frame(Pointer* /* pointer */,
                            wl_pointer* /* wl_pointer */) override {}

  void notify_pointer_axis_source(Pointer* /* pointer */,
                                  wl_pointer* /* wl_pointer */,
                                  uint32_t /* axis_source */) override {}

  void notify_pointer_axis_stop(Pointer* /* pointer */,
                                wl_pointer* /* wl_pointer */,
                                uint32_t /* time */,
                                uint32_t /* axis */) override {}

  void notify_pointer_axis_discrete(Pointer* /* pointer */,
                                    wl_pointer* /* wl_pointer */,
                                    uint32_t /* axis */,
                                    int32_t /* discrete */) override {}

 private:
  wl_display* wl_display_{};
  std::unique_ptr<Logging> logging_{};
  std::shared_ptr<WindowManager> wm_;
  Seat* seat_{};
  std::shared_ptr<XdgTopLevel> toplevel_;
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(const int argc, char** argv) {
  std::signal(SIGINT, handle_signal);

  cxxopts::Options options("simple-csd",
                           "waypp CSD example — client-side decorations");
  // clang-format off
  options.add_options()
      ("w,width",           "Content width",
                            cxxopts::value<int>()->default_value("600"))
      ("h,height",          "Content height",
                            cxxopts::value<int>()->default_value("400"))
      ("c,disable-cursor",  "Disable cursor")
      ("f,fullscreen",      "Run fullscreen (hides decorations)")
      ("m,maximized",       "Start maximized (hides decorations)")
      ("r,fullscreen-ratio","Maintain aspect ratio in fullscreen")
      ("t,tearing",         "Enable tearing_control protocol")
      ("n,no-csd",          "Disable client-side decorations")
      ("T,title",           "Window title shown in the decoration title bar",
                            cxxopts::value<std::string>()->default_value(
                                "waypp CSD demo"));
  // clang-format on

  const auto result = options.parse(argc, argv);

  try {
    const App app({
        .width = result["width"].as<int>(),
        .height = result["height"].as<int>(),
        .disable_cursor = result["disable-cursor"].as<bool>(),
        .fullscreen = result["fullscreen"].as<bool>(),
        .maximized = result["maximized"].as<bool>(),
        .fullscreen_ratio = result["fullscreen-ratio"].as<bool>(),
        .tearing = result["tearing"].as<bool>(),
        .no_csd = result["no-csd"].as<bool>(),
        .title = result["title"].as<std::string>(),
    });

    while (gRunning.load(std::memory_order_acquire) && app.run()) {
    }
  } catch (const std::runtime_error& e) {
    spdlog::critical("Fatal error: {}", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
