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
#include <chrono>
#include <csignal>
#include <stdexcept>

#include <GLES3/gl32.h>
#include <linux/input.h>
#include <cxxopts.hpp>
#include <glm/glm.hpp>

#include "logging/logging.h"
#include "shaders/glsl-ray-tracing-shaders.h"
#include "waypp/window/xdg_toplevel.h"

class App;

/// Use std::atomic<bool> — volatile provides no memory-ordering guarantee for
/// signal-handler / main-thread sharing (the same class of bug as
/// HIGH-2/HIGH-3).
static std::atomic<bool> running{true};
static std::atomic<bool> scene_initialized{false};

static constexpr int kResizeMargin = 12;

/// EGL Context Attribute configuration
std::array<EGLint, 7> kEglContextAttribs1 = {{
    // clang-format off
                EGL_CONTEXT_MAJOR_VERSION, 3,
                EGL_CONTEXT_MINOR_VERSION, 3,
                EGL_CONTEXT_OPENGL_PROFILE_MASK,
                EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                EGL_NONE,
    // clang-format on
}};

/// EGL Configuration Attributes
std::array<EGLint, 21> kEglConfigAttribs1 = {{
    // clang-format off
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_CONFORMANT, EGL_OPENGL_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_DEPTH_SIZE, 24,
                EGL_STENCIL_SIZE, 8,
                //EGL_SAMPLE_BUFFERS, 1,
                //EGL_SAMPLES, 4, // 4x MSAA
                EGL_NONE,
    // clang-format on
}};

struct Configuration {
  int width;
  int height;
  bool fullscreen;
  int maximized;
  bool fullscreen_ratio;
  bool tearing;
  int delay;
  bool opaque;
  int interval;
  /// Fraction of window resolution to render the shader at (0.0 < s <= 1.0).
  /// Values < 1.0 reduce fragment-shader loads significantly; e.g., 0.5 renders
  /// at quarter the pixel count (half width × half height).
  float render_scale;
} config;

/// All GL context states — kept as a plain struct, so lifetime is explicit.
struct Context {
  GLuint shader_program{};
  GLuint VAO{};
  int render_width{};
  int render_height{};
  /// Cached uniform locations — obtained once at init, used every frame.
  /// glGetUniformLocation() causes an implicit GPU pipeline flush on many
  /// drivers; calling it per-frame is a significant-hidden bottleneck.
  GLint loc_iTime{-1};
  GLint loc_iResolution{-1};
  GLint loc_iMouse{-1};
  /// Wall-clock time at the first frame — used for monotonically increasing
  /// iTime.
  std::chrono::steady_clock::time_point start_time{};
  /// FPS tracking
  uint32_t frame_count{};
  std::chrono::steady_clock::time_point fps_epoch{};
} ctx;

void handle_signal(const int signal) {
  if (signal == SIGINT) {
    running.store(false, std::memory_order_relaxed);
  }
}

/// Load and compile a GLSL shader. Throws std::runtime_error on failure
/// instead of calling exit(), so RAII cleanup runs normally (fixes the
/// exit()-in-non-constructor pattern from the CRIT-3 family of bugs).
GLuint load_shader(const GLchar* shader_source, const GLenum shader_type) {
  const GLuint shader = glCreateShader(shader_type);
  if (shader == 0)
    throw std::runtime_error("glCreateShader returned 0");

  glShaderSource(shader, 1, &shader_source, nullptr);
  glCompileShader(shader);

  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::string log;
    if (len > 1) {
      const auto buf = std::make_unique<char[]>(static_cast<size_t>(len));
      glGetShaderInfoLog(shader, len, nullptr, buf.get());
      log.assign(buf.get(), static_cast<size_t>(len));
    }
    glDeleteShader(shader);
    throw std::runtime_error("[gl shader] compile failed: " + log);
  }
  return shader;
}

void initialize_scene(Window* window) {
  /// Full-screen quad covering NDC [-1,1].
  constexpr float quad_vertices[] = {
      -1.0f, -1.0f, 0.0f, 0.0f,  -1.0f, 1.0f,
      0.0f,  1.0f,  1.0f, -1.0f, 1.0f,  0.0f,

      1.0f,  -1.0f, 1.0f, 0.0f,  -1.0f, 1.0f,
      0.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f,
  };

  window->make_current();

  glGenVertexArrays(1, &ctx.VAO);
  glBindVertexArray(ctx.VAO);

  GLuint VBO = 0;
  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void*>(0));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void*>(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);
  // VBO remains bound to the VAO; it is not separately tracked.

  ctx.render_width = static_cast<int>(static_cast<float>(window->get_width()) *
                                      config.render_scale);
  ctx.render_height = static_cast<int>(
      static_cast<float>(window->get_height()) * config.render_scale);
  // Ensure at least 1×1.
  ctx.render_width = std::max(1, ctx.render_width);
  ctx.render_height = std::max(1, ctx.render_height);

  // Compile and link the shader program.
  const auto vertex_shader =
      load_shader(vertex_shader_source, GL_VERTEX_SHADER);
  const auto fragment_shader =
      load_shader(fragment_shader_source, GL_FRAGMENT_SHADER);

  ctx.shader_program = glCreateProgram();
  glAttachShader(ctx.shader_program, vertex_shader);
  glAttachShader(ctx.shader_program, fragment_shader);
  glLinkProgram(ctx.shader_program);

  GLint linked = 0;
  glGetProgramiv(ctx.shader_program, GL_LINK_STATUS, &linked);
  if (!linked) {
    GLint len = 0;
    glGetProgramiv(ctx.shader_program, GL_INFO_LOG_LENGTH, &len);
    std::string log;
    if (len > 1) {
      const auto buf = std::make_unique<char[]>(static_cast<size_t>(len));
      glGetProgramInfoLog(ctx.shader_program, len, nullptr, buf.get());
      log.assign(buf.get(), static_cast<size_t>(len));
    }
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    throw std::runtime_error("[gl] link failed: " + log);
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  glUseProgram(ctx.shader_program);

  // Cache uniform locations once — avoids the per-frame GPU stall caused by
  // calling glGetUniformLocation() inside the render loop.
  ctx.loc_iTime = glGetUniformLocation(ctx.shader_program, "iTime");
  ctx.loc_iResolution = glGetUniformLocation(ctx.shader_program, "iResolution");
  ctx.loc_iMouse = glGetUniformLocation(ctx.shader_program, "iMouse");

  const glm::vec2 screen(static_cast<float>(ctx.render_width),
                         static_cast<float>(ctx.render_height));
  if (ctx.loc_iResolution >= 0)
    glUniform2fv(ctx.loc_iResolution, 1, &screen[0]);

  ctx.start_time = std::chrono::steady_clock::now();
  ctx.fps_epoch = ctx.start_time;
  ctx.frame_count = 0;
}

/**
 * @brief Handles a window resize by updating the render dimensions and the
 *        iResolution uniform. The viewport is also updated in draw_frame.
 */
static void resize_scene(Window* window) {
  ctx.render_width = static_cast<int>(static_cast<float>(window->get_width()) *
                                      config.render_scale);
  ctx.render_height = static_cast<int>(
      static_cast<float>(window->get_height()) * config.render_scale);
  ctx.render_width = std::max(1, ctx.render_width);
  ctx.render_height = std::max(1, ctx.render_height);

  DLOG_DEBUG("[gl-shadertoy] resize_scene {}x{}", ctx.render_width,
             ctx.render_height);

  glUseProgram(ctx.shader_program);
  const glm::vec2 screen(static_cast<float>(ctx.render_width),
                         static_cast<float>(ctx.render_height));
  if (ctx.loc_iResolution >= 0)
    glUniform2fv(ctx.loc_iResolution, 1, &screen[0]);
}

/**
 * @brief Per-frame render callback.
 *
 * Performance notes:
 *  - iTime is a smooth float elapsed seconds — no integer truncation.
 *  - Uniform locations are cached; no glGetUniformLocation per frame.
 *  - The off-screen FBO pass has been removed; the shader renders directly
 *    to the default framebuffer, halving GPU memory bandwidth per frame.
 *  - swap_interval=0 (--non-blocking flag) removes vsync cap entirely.
 *  - render_scale < 1.0 (--render-scale flag) reduces fragment workload.
 */
static void draw_frame(void* userdata, uint32_t /* time */) {
  const auto window = static_cast<Window*>(userdata);

  window->update_buffer_geometry();

  if (!scene_initialized.load(std::memory_order_acquire)) {
    initialize_scene(window);
    scene_initialized.store(true, std::memory_order_release);
  }

  // Detect resize and update size-dependent state.
  const int target_w =
      std::max(1, static_cast<int>(static_cast<float>(window->get_width()) *
                                   config.render_scale));
  const int target_h =
      std::max(1, static_cast<int>(static_cast<float>(window->get_height()) *
                                   config.render_scale));
  if (target_w != ctx.render_width || target_h != ctx.render_height) {
    resize_scene(window);
  }

  // Smooth elapsed time in seconds — no integer cast that quantises to
  // 1-second steps (was the original iTime bug causing jerky animation).
  const float elapsed = std::chrono::duration<float>(
                            std::chrono::steady_clock::now() - ctx.start_time)
                            .count();

  // Render directly to the default framebuffer.
  // The previous code bound an off-screen FBO, cleared it, then immediately
  // unbound it and re-drew with the same shader to the default framebuffer —
  // doubling GPU work without producing any visible difference.
  glViewport(0, 0, ctx.render_width, ctx.render_height);
  glUseProgram(ctx.shader_program);

  if (ctx.loc_iTime >= 0)
    glUniform1f(ctx.loc_iTime, elapsed);

  glBindVertexArray(ctx.VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  window->swap_buffers();

  // FPS counter — printed every 5 seconds.
  ++ctx.frame_count;
  const auto now = std::chrono::steady_clock::now();
  const float fps_elapsed =
      std::chrono::duration<float>(now - ctx.fps_epoch).count();
  if (fps_elapsed >= 5.0f) {
    spdlog::info("[gl-shadertoy] {:.1f} FPS  (render {}x{})",
                 static_cast<float>(ctx.frame_count) / fps_elapsed,
                 ctx.render_width, ctx.render_height);
    ctx.frame_count = 0;
    ctx.fps_epoch = now;
  }
}

class EventObserver : public SeatObserver,
                      public KeyboardObserver,
                      public PointerObserver {
 public:
  explicit EventObserver(XdgTopLevel* toplevel, Seat** seat_out)
      : toplevel_(toplevel), seat_out_(seat_out) {}

  void notify_seat_capabilities(Seat* seat,
                                wl_seat* /* seat */,
                                uint32_t /* caps */) override {
    if (seat) {
      *seat_out_ = seat;
      if (const auto keyboard = seat->get_keyboard(); keyboard.has_value()) {
        keyboard.value()->register_observer(this);
      }
      if (const auto pointer = seat->get_pointer(); pointer.has_value()) {
        pointer.value()->register_observer(this);
      }
    }
  }

  void notify_seat_name(Seat* /* seat */,
                        wl_seat* /* seat */,
                        const char* name) override {
    spdlog::info("Seat: {}", name);
  }

  // ── KeyboardObserver ──────────────────────────────────────────────────

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
      uint32_t state,
      int xdg_key_symbol_count,
      const xkb_keysym_t* xdg_key_symbols) override {
    spdlog::info(
        "Key: serial: {}, time: {}, xkb_scancode: 0x{:X}, key_repeats: {}, "
        "state: {}, xdg_keysym_count: {}, syms_out[0]: 0x{:X}",
        serial, time, xkb_scancode, key_repeats,
        state == KeyState::KEY_STATE_PRESS ? "press" : "release",
        xdg_key_symbol_count, xdg_key_symbols[0]);
  }

  // ── PointerObserver ───────────────────────────────────────────────────

  void notify_pointer_enter(Pointer* pointer,
                            wl_pointer* /* wl_pointer */,
                            uint32_t serial,
                            wl_surface* /* surface */,
                            double /* sx */,
                            double /* sy */) override {
    pointer->set_cursor(serial, "left_ptr");
  }

  void notify_pointer_leave(Pointer* /* pointer */,
                            wl_pointer* /* wl_pointer */,
                            uint32_t /* serial */,
                            wl_surface* /* surface */) override {}

  void notify_pointer_motion(Pointer* /* pointer */,
                             wl_pointer* /* wl_pointer */,
                             uint32_t /* time */,
                             double /* sx */,
                             double /* sy */) override {}

  void notify_pointer_button(Pointer* pointer,
                             wl_pointer* /* wl_pointer */,
                             uint32_t serial,
                             uint32_t /* time */,
                             uint32_t button,
                             uint32_t state) override {
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED &&
        *seat_out_) {
      if (const auto edge = toplevel_->check_edge_resize(pointer->get_xy());
          edge != XDG_TOPLEVEL_RESIZE_EDGE_NONE) {
        toplevel_->resize((*seat_out_)->get_seat(), serial, edge);
      }
    }
  }

  void notify_pointer_axis(Pointer* /* pointer */,
                           wl_pointer* /* wl_pointer */,
                           uint32_t /* time */,
                           uint32_t /* axis */,
                           double /* value */) override {}

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
  XdgTopLevel* toplevel_;
  Seat** seat_out_;
};

/**
 * @brief Main function for the program.
 *
 * This function initializes the surface manager and creates a surface with the
 * specified dimensions and type. It sets up a signal handler for SIGINT
 * (Ctrl+C) to stop the program and then enters a loop to handle surface
 * events.
 *
 * @param argc The number of command line arguments.
 * @param argv An array of strings representing the command line arguments.
 * @return An integer representing the exit status of the program.
 */
int main(int argc, char** argv) {
  auto log_init = std::make_unique<Logging>();

  auto display = wl_display_connect(nullptr);
  if (!display) {
    spdlog::critical("Unable to connect to Wayland socket.");
    return EXIT_FAILURE;
  }

  std::signal(SIGINT, handle_signal);

  cxxopts::Options options("gl-shadertoy", "OpenGL Shadertoy");
  options.add_options()
      // clang-format off
            ("w,width",        "Set width",                    cxxopts::value<int>()->default_value("250"))
            ("h,height",       "Set height",                   cxxopts::value<int>()->default_value("250"))
            ("f,fullscreen",   "Run in fullscreen mode")
            ("m,maximized",    "Run in maximized mode")
            ("r,fullscreen-ratio", "Use fixed width/height ratio when run in fullscreen mode")
            ("t,tearing",      "Enable tearing via the tearing_control protocol")
            ("d,delay",        "Buffer swap delay in microseconds", cxxopts::value<int>()->default_value("0"))
            ("o,opaque",       "Create an opaque surface")
            ("i,interval",     "Set eglSwapInterval to interval",   cxxopts::value<int>()->default_value("1"))
            ("b,non-blocking", "Don't sync to compositor redraw (eglSwapInterval 0; removes vsync cap)")
            ("s,render-scale", "Render at this fraction of window resolution (0.0 < s <= 1.0; "
                               "e.g. 0.5 = quarter pixel count)",
                               cxxopts::value<float>()->default_value("1.0"));
  // clang-format on
  const auto result = options.parse(argc, argv);

  const float render_scale =
      std::max(0.1f, std::min(1.0f, result["render-scale"].as<float>()));

  config = {
      .width = result["width"].as<int>(),
      .height = result["height"].as<int>(),
      .fullscreen = result["fullscreen"].as<bool>(),
      .maximized = result["maximized"].as<bool>(),
      .fullscreen_ratio = result["fullscreen-ratio"].as<bool>(),
      .tearing = result["tearing"].as<bool>(),
      .delay = result["delay"].as<int>(),
      .opaque = result["opaque"].as<bool>(),
      .interval =
          result["non-blocking"].as<bool>() ? 0 : result["interval"].as<int>(),
      .render_scale = render_scale,
  };

  if (config.opaque) {
    kEglConfigAttribs1[15] = 0;
  }

  try {
    auto wm = std::make_shared<XdgWindowManager>(display);

    waypp::Egl::config egl_config{};
    egl_config.context_attribs_size = kEglContextAttribs1.size();
    egl_config.context_attribs = kEglContextAttribs1.data();
    egl_config.config_attribs_size = kEglConfigAttribs1.size();
    egl_config.config_attribs = kEglConfigAttribs1.data();
    egl_config.buffer_bpp = 32;
    egl_config.swap_interval = config.interval;
    egl_config.type = waypp::Egl::OPENGL_API;

    auto top_level = wm->create_top_level(
        "gl-shadertoy", "org.freedesktop.gitlab.jwinarske.waypp.gl-shadertoy",
        config.width, config.height, kResizeMargin, 0, 0, config.fullscreen,
        config.maximized, config.fullscreen_ratio, config.tearing, draw_frame,
        &egl_config);

    Seat* seat = nullptr;
    const auto event_observer =
        std::make_unique<EventObserver>(top_level.get(), &seat);
    if (const auto seat_opt = wm->get_seat(); seat_opt.has_value()) {
      seat_opt.value()->register_observer(event_observer.get());
    }

    top_level->start_frame_callbacks();

    while (running.load(std::memory_order_acquire) && top_level->is_valid() &&
           wm->display_dispatch() != -1) {
    }

    top_level.reset();
    wm.reset();
  } catch (const std::runtime_error& e) {
    spdlog::critical("Fatal error: {}", e.what());
    wl_display_flush(display);
    wl_display_disconnect(display);
    return EXIT_FAILURE;
  }

  wl_display_flush(display);
  wl_display_disconnect(display);

  return EXIT_SUCCESS;
}
