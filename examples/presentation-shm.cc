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

#include <algorithm>
#include <csignal>
#include <thread>
#include <vector>

#include <cxxopts.hpp>

#include "logging/logging.h"
#include "presentation-time-client-protocol.h"
#include "window/xdg_toplevel.h"

enum run_mode {
  RUN_MODE_FEEDBACK,
  RUN_MODE_FEEDBACK_IDLE,
  RUN_MODE_PRESENT,
};

static constexpr char run_mode_name[3][16] = {
    "feedback",
    "feedback-idle",
    "low-lat present",
};

static constexpr int kResizeMargin = 12;

struct Configuration {
  int width;
  int height;
  enum run_mode mode;
  int refresh_nsec;
  int commit_delay_msecs;
};

struct Feedback {
  Window* window;
  struct wp_presentation_feedback* wp_presentation_feedback;
  unsigned frame_no;
  struct timespec commit;
  struct timespec target;
  uint32_t frame_stamp;
  struct timespec present;
};

struct Context {
  struct wl_display* display;
  std::shared_ptr<XdgWindowManager> wm;
  std::shared_ptr<XdgTopLevel> toplevel;
  Configuration config;
  std::vector<std::unique_ptr<Feedback>> feedback_list;
};

static constexpr int kBufferCount = 60;
static constexpr uint32_t kNanoSecondPerSecond = 1000000000;

static volatile bool running = true;

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
    running = false;
  }
}

static void paint_pixels(void* image, int width, int height, uint32_t phase) {
  const int halfh = height / 2;
  const int halfw = width / 2;
  uint32_t* pixel = static_cast<uint32_t*>(image);
  int y, or_;
  double ang = M_PI * 2.0 / 1000000.0 * phase;
  double s = sin(ang);
  double c = cos(ang);

  /// squared radii thresholds
  or_ = (halfw < halfh ? halfw : halfh) - 16;
  or_ *= or_;

  for (y = 0; y < height; y++) {
    int x;
    int oy = y - halfh;
    int y2 = oy * oy;

    for (x = 0; x < width; x++) {
      int ox = x - halfw;
      uint32_t v = 0xff000000;
      double rx, ry;

      if (ox * ox + y2 > or_) {
        if (ox * oy > 0)
          *pixel++ = 0xff000000;
        else
          *pixel++ = 0xffffffff;
        continue;
      }

      rx = c * ox + s * oy;
      ry = -s * ox + c * oy;

      if (rx < 0.0)
        v |= 0x00ff0000;
      if (ry < 0.0)
        v |= 0x0000ff00;
      if ((rx < 0.0) == (ry < 0.0))
        v |= 0x000000ff;

      *pixel++ = v;
    }
  }
}

static void feedback_sync_output(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    struct wl_output* output) {
  auto ctx = static_cast<struct Feedback*>(data);
  if (ctx->wp_presentation_feedback != wp_presentation_feedback) {
    return;
  }
  DLOG_DEBUG("feedback_sync_output: 0x{:x}", fmt::ptr(output));
  (void)output;
}

static void feedback_presented(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    uint32_t tv_sec_hi,
    uint32_t tv_sec_lo,
    uint32_t tv_nsec,
    uint32_t refresh_nsec,
    uint32_t seq_hi,
    uint32_t seq_lo,
    uint32_t flags) {
  auto ctx = static_cast<struct Feedback*>(data);
  if (ctx->wp_presentation_feedback != wp_presentation_feedback) {
    return;
  }

  auto seconds = (static_cast<uint64_t>(tv_sec_hi) << 32) + tv_sec_lo;
  auto duration =
      std::chrono::seconds{seconds} + std::chrono::nanoseconds{tv_nsec};

  uint64_t seq = (static_cast<uint64_t>(seq_hi) << 32) + seq_lo;

  LOG_DEBUG(
      "feedback_presented: ts: {} nS, refresh: {} nS, sequence: {}, flags: {}",
      std::chrono::nanoseconds(duration).count(), refresh_nsec, seq, flags);
}

static void destroy_feedback(struct Feedback* feedback) {
  if (feedback->wp_presentation_feedback) {
    wp_presentation_feedback_destroy(feedback->wp_presentation_feedback);
  }

  auto ctx = static_cast<Context*>(feedback->window->get_user_data());

  auto idx = std::find_if(
      ctx->feedback_list.begin(), ctx->feedback_list.end(),
      [&](std::unique_ptr<Feedback> const& s) { return s.get() == feedback; });

  ctx->feedback_list.erase(idx);
}

static void feedback_discarded(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback) {
  auto feedback = static_cast<struct Feedback*>(data);
  if (feedback->wp_presentation_feedback != wp_presentation_feedback) {
    return;
  }

  spdlog::info("feedback_discarded {}", feedback->frame_no);

  destroy_feedback(feedback);
}

static constexpr struct wp_presentation_feedback_listener feedback_listener = {
    .sync_output = feedback_sync_output,
    .presented = feedback_presented,
    .discarded = feedback_discarded,
};

void create_feedback(Window* window, uint32_t time) {
  LOG_DEBUG("create_feedback: {}", time);
  static unsigned seq = 0;
  auto* ctx = static_cast<Context*>(window->get_user_data());
  LOG_DEBUG("context: {}", fmt::ptr(ctx));

  auto feedback = std::make_unique<Feedback>();
  feedback->window = window;
  feedback->wp_presentation_feedback = wp_presentation_feedback(
      ctx->wm->get_wp_presentation(), window->get_surface());
  wp_presentation_feedback_add_listener(feedback->wp_presentation_feedback,
                                        &feedback_listener, feedback.get());

  feedback->frame_no = ++seq;

  clock_gettime(ctx->wm->get_clk_id(), &feedback->commit);
  feedback->target = feedback->commit;
  feedback->frame_stamp = time;

  ctx->feedback_list.push_back(std::move(feedback));
}

static void emulate_rendering(Window* window) {
  LOG_DEBUG("emulate_rendering");
  auto* config = static_cast<Configuration*>(window->get_user_data());
  if (config->commit_delay_msecs <= 0) {
    return;
  }

  std::chrono::milliseconds ms(config->commit_delay_msecs);
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(ms);
  std::this_thread::sleep_for(ns);
}

static void redraw_mode_feedback(void* data, uint32_t time) {
  auto window = static_cast<Window*>(data);

  emulate_rendering(window);
  create_feedback(window, time);
}

int main(const int argc, char** argv) {
  std::signal(SIGINT, handle_signal);

  cxxopts::Options options("simple-presentation",
                           "Weston simple-presentation example");
  options.add_options()
      // clang-format off
            ("f,feedback", "run in feedback mode (default)")
            ("i,feedback-idle", "run in feedback-idle mode; sleep 1s between frames")
            ("p,present", "run in low-latency presentation mode")
            ("d,msecs", "emulate the time used for rendering by a delay of the given milliseconds before commit",
             cxxopts::value<int>()->default_value("0"))
            ("w,width", "Set width", cxxopts::value<int>()->default_value("250"))
            ("h,height", "Set height", cxxopts::value<int>()->default_value("250"));

  // clang-format on
  auto result = options.parse(argc, argv);

  auto log_init = std::make_unique<Logging>();
  auto ctx = std::make_unique<Context>();

  ctx->config = {
      .width = result["width"].as<int>(),
      .height = result["height"].as<int>(),
      .commit_delay_msecs = result["msecs"].as<int>(),
  };

  ctx->config.mode = RUN_MODE_FEEDBACK;
  if (result["feedback"].as<bool>()) {
    ctx->config.mode = RUN_MODE_FEEDBACK;
  } else if (result["feedback-idle"].as<bool>()) {
    ctx->config.mode = RUN_MODE_FEEDBACK_IDLE;
  } else if (result["present"].as<bool>()) {
    ctx->config.mode = RUN_MODE_PRESENT;
  }

  ctx->config.refresh_nsec = kNanoSecondPerSecond / 60;

  ctx->display = wl_display_connect(nullptr);
  if (!ctx->display) {
    spdlog::critical("Unable to connect to Wayland socket.");
    exit(EXIT_FAILURE);
  }

  ctx->wm = std::make_unique<XdgWindowManager>(ctx->display);

  spdlog::info("XDG Window Manager Version: {}", ctx->wm->get_version());

  std::stringstream title;
  title << "presentation-shm: " << run_mode_name[ctx->config.mode] << "[Delay "
        << ctx->config.commit_delay_msecs << " msecs]";
  LOG_DEBUG("title: {}", title.str().c_str());

  ctx->toplevel = ctx->wm->create_top_level(
      title.str().c_str(), "jwinarske.waypp.simple_presentation",
      ctx->config.width, ctx->config.height, kResizeMargin, kBufferCount,
      WL_SHM_FORMAT_XRGB8888, false, false, false, false, redraw_mode_feedback);
  spdlog::info("XDG Window Version: {}", ctx->toplevel->get_version());

  LOG_DEBUG("context: {}", fmt::ptr(&ctx->config));
  ctx->toplevel->set_user_data(&ctx->config);
  ctx->toplevel->set_min_size(ctx->config.width, ctx->config.height);
  ctx->toplevel->set_max_size(ctx->config.width, ctx->config.height);

  /// pre-render
  auto& buffers = ctx->toplevel->get_buffers();
  static constexpr uint32_t time_factor = 1000000 / kBufferCount;

  uint32_t i = 0;
  for (auto& buffer : buffers) {
    if (buffer->is_busy()) {
      spdlog::error("wl_buffer id {} is busy", buffer->get_id());
    }
    paint_pixels(buffer->get_shm_data(), buffer->get_width(),
                 buffer->get_height(), i * time_factor);
    i++;
  }

  switch (ctx->config.mode) {
    case RUN_MODE_FEEDBACK:
    case RUN_MODE_FEEDBACK_IDLE:
      ctx->toplevel->start_frame_callbacks();
      break;
    case RUN_MODE_PRESENT:
      //            firstdraw_mode_burst(window);
      break;
  }

  while (running && ctx->toplevel->is_valid() &&
         ctx->wm->display_dispatch() != -1) {
  }

  ctx->toplevel.reset() ctx->wm.reset();

  wl_display_flush(ctx->display);
  wl_display_disconnect(ctx->display);

  return EXIT_SUCCESS;
}