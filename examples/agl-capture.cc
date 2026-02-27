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

#include <atomic>
#include <memory>
#include <stdexcept>

#include <cstdint>
#include <cxxopts.hpp>

#include "logging/logging.h"
#include "window_manager/agl_shell.h"
#include "window_manager/weston-capture.h"

struct Configuration {
  bool write_back;
  bool frame_buffer;
  bool full_frame_buffer;
  bool blending;
  std::string output;
  bool list;
  bool all;
};

static std::atomic<bool> gRunning{true};

class App final : public WestonCaptureObserver {
 public:
  explicit App(const Configuration& config) : weston_capture_v1_(nullptr) {
    display_ = wl_display_connect(nullptr);
    if (!display_) {
      throw std::runtime_error("Unable to connect to Wayland socket.");
    }

    agl_shell_ = std::make_shared<AglShell>(display_, false);
    const auto d = agl_shell_->get_display();

    // required when not creating a window
    wl_display_roundtrip(d);

    if (config.list) {
      auto& outputs = agl_shell_->get_outputs();
      for (const auto& output : outputs) {
        spdlog::info("Output: {}", output.second->get_name());
      }
      // Signal the run loop to exit immediately after listing.
      gRunning.store(false, std::memory_order_relaxed);
      return;
    }

    /// Weston Capture
    weston_capture_v1_ = agl_shell_->get_weston_capture_v1();
    if (!weston_capture_v1_) {
      throw std::runtime_error("weston_capture_v1 interface not found.");
    }

    /// Source
    weston_capture_v1_source source = WESTON_CAPTURE_V1_SOURCE_FULL_FRAMEBUFFER;
    if (config.write_back) {
      source = WESTON_CAPTURE_V1_SOURCE_WRITEBACK;
    } else if (config.frame_buffer) {
      source = WESTON_CAPTURE_V1_SOURCE_FRAMEBUFFER;
    } else if (config.full_frame_buffer) {
      source = WESTON_CAPTURE_V1_SOURCE_FULL_FRAMEBUFFER;
    } else if (config.blending) {
      source = WESTON_CAPTURE_V1_SOURCE_BLENDING;
    }

    /// Output
    if (!config.all) {
      wl_output* output{};

      if (!config.output.empty()) {
        output = agl_shell_->find_output_by_name(config.output);
      } else {
        output = agl_shell_->get_primary_output();
      }

      if (!output) {
        throw std::runtime_error("Output not available.");
      }

      weston_capture_list_.push_back(std::make_unique<WestonCapture>(
          weston_capture_v1_, output, source, this, this));
    } else {
      for (const auto& output : agl_shell_->get_outputs()) {
        weston_capture_list_.push_back(std::make_unique<WestonCapture>(
            weston_capture_v1_, output.first, source, this, this));
      }
    }

    spdlog::info("AGL Capture");
  }

  void notify_weston_capture_format(
      void* user_data,
      weston_capture_source_v1* /* weston_capture_source_v1 */,
      uint32_t drm_format) override {
    const auto obj = static_cast<App*>(user_data);
    obj->buffer_.drm_format = drm_format;
    spdlog::debug("drm_format: 0x{:X}", drm_format);
  }

  void notify_weston_capture_size(
      void* user_data,
      weston_capture_source_v1* /* weston_capture_source_v1 */,
      int32_t width,
      int32_t height) override {
    const auto obj = static_cast<App*>(user_data);
    obj->buffer_.width = width;
    obj->buffer_.height = height;
    spdlog::debug("width: {}, height: {}", width, height);
  }

  void notify_weston_capture_complete(
      void* /* user_data */,
      weston_capture_source_v1* /* weston_capture_source_v1 */) override {
    gRunning.store(false, std::memory_order_relaxed);
    spdlog::debug("complete");
  }

  void notify_weston_capture_retry(
      void* /* user_data */,
      weston_capture_source_v1* /* weston_capture_source_v1 */) override {
    spdlog::debug("retry");
  }

  void notify_weston_capture_failed(
      void* /* user_data */,
      weston_capture_source_v1* /* weston_capture_source_v1 */,
      const char* msg) override {
    spdlog::debug("failed: {}", msg);
  }

  ~App() override {
    agl_shell_.reset();
    if (display_) {
      wl_display_flush(display_);
      wl_display_disconnect(display_);
    }
  }

  [[nodiscard]] bool run() const {
    /// display_dispatch is blocking
    return (gRunning.load(std::memory_order_acquire) &&
            agl_shell_->display_dispatch() != -1);
  }

 private:
  wl_display* display_{};
  std::unique_ptr<Logging> logging_{};
  std::shared_ptr<AglShell> agl_shell_;
  std::list<std::unique_ptr<WestonCapture>> weston_capture_list_;
  weston_capture_v1* weston_capture_v1_{};

  struct {
    uint32_t drm_format{};
    int32_t width{};
    int32_t height{};
  } buffer_{};
};

int main(const int argc, char** argv) {
  auto log_init = std::make_unique<Logging>();

  cxxopts::Options options("agl-capture", "AGL Output Capture Utility");
  options.add_options()
      // clang-format off
            ("w,writeback", "Use hardware writeback")
            ("d,framebuffer", "Copy from framebuffer, desktop area")
            ("f,full-framebuffer", "Copy whole framebuffer, including borders")
            ("b,blending", "Copy from blending space")
            ("o,output", "take a screenshot of the output specified by OUTPUT_NAME", cxxopts::value<std::string>())
            ("l,list", "list all the outputs found")
            ("a,all", "take a screenshot of all the outputs found");
  // clang-format on
  const auto result = options.parse(argc, argv);

  try {
    const App app({
        .write_back = result["writeback"].as<bool>(),
        .frame_buffer = result["framebuffer"].as<bool>(),
        .full_frame_buffer = result["full-framebuffer"].as<bool>(),
        .blending = result["blending"].as<bool>(),
        .output =
            result.count("output") ? result["output"].as<std::string>() : "",
        .list = result["list"].as<bool>(),
        .all = result["all"].as<bool>(),
    });

    while (app.run()) {
    }
  } catch (const std::runtime_error& e) {
    spdlog::critical("Fatal error: {}", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}