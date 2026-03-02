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
#include "waypp/window_manager/window_manager_factory.h"

class App;

static std::atomic<bool> running{true};
static std::atomic<bool> scene_initialized{false};

static constexpr int kResizeMargin = 12;

std::array<EGLint, 7> kEglContextAttribs1 = {{
    // clang-format off
    EGL_CONTEXT_MAJOR_VERSION, 3,
    EGL_CONTEXT_MINOR_VERSION, 3,
    EGL_CONTEXT_OPENGL_PROFILE_MASK,
    EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
    EGL_NONE,
    // clang-format on
}};

std::array<EGLint, 21> kEglConfigAttribs1 = {{
    // clang-format off
    EGL_SURFACE_TYPE,       EGL_WINDOW_BIT,
    EGL_CONFORMANT,         EGL_OPENGL_BIT,
    EGL_RENDERABLE_TYPE,    EGL_OPENGL_BIT,
    EGL_COLOR_BUFFER_TYPE,  EGL_RGB_BUFFER,
    EGL_RED_SIZE,   8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE,  8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 24,
    EGL_STENCIL_SIZE, 8,
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
  float render_scale;
  /// Samples traced per frame (1 = max fps; higher = less noise per frame).
  int samples_per_frame;
  /// Maximum path-tracing bounce depth (lower = faster).
  int max_bounce;
  /// EMA blend weight for temporal accumulation (0.0–1.0).
  /// Higher = more responsive to motion, more noise.
  /// Lower  = smoother, more ghosting on fast motion.
  float ema_weight;
} config;

// ---------------------------------------------------------------------------
// GL context state — three-pass pipeline:
//   Pass 1: ray-trace → tex_new   (FBO: fbo_trace)
//   Pass 2: accumulate → tex_accum_write, reading tex_accum_read + tex_new
//           (ping-pong between two accumulation textures)
//   Pass 3: display tex_accum_read → default framebuffer
// ---------------------------------------------------------------------------
struct Context {
  // Geometry
  GLuint VAO{};
  GLuint VBO{};       // tracked so we can delete it on resize/teardown
  GLuint quad_VAO{};  // accumulation / display quad (no tex coords needed)
  GLuint quad_VBO{};

  // Ray-trace pass
  GLuint fbo_trace{};
  GLuint tex_new{};  // RGBA16F — output of the ray-trace pass
  GLuint prog_trace{};

  // Accumulation pass (ping-pong)
  GLuint fbo_accum[2]{};
  GLuint tex_accum[2]{};
  GLuint prog_accum{};
  int accum_write{0};

  // Display pass
  GLuint prog_display{};

  // Uniform locations — trace pass
  GLint loc_iTime{-1};
  GLint loc_iResolution{-1};
  GLint loc_iMouse{-1};
  GLint loc_iSamplesPerFrame{-1};
  GLint loc_iMaxBounce{-1};

  // Uniform locations — accumulation pass
  GLint loc_accum_newFrame{-1};
  GLint loc_accum_accumTex{-1};
  GLint loc_accum_blendWeight{-1};

  // Uniform locations — display pass
  GLint loc_display_accumTex{-1};

  int render_width{};
  int render_height{};

  // Timing / FPS
  std::chrono::steady_clock::time_point start_time{};
  std::chrono::steady_clock::time_point fps_epoch{};
  uint32_t frame_count{};
} ctx;

void handle_signal(const int signal) {
  if (signal == SIGINT) {
    running.store(false, std::memory_order_relaxed);
  }
}

// ---------------------------------------------------------------------------
// Shader helpers
// ---------------------------------------------------------------------------
static GLuint compile_shader(const GLchar* src, GLenum type) {
  GLuint sh = glCreateShader(type);
  if (!sh)
    throw std::runtime_error("glCreateShader returned 0");
  glShaderSource(sh, 1, &src, nullptr);
  glCompileShader(sh);
  GLint ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
    std::string log;
    if (len > 1) {
      auto buf = std::make_unique<char[]>(static_cast<size_t>(len));
      glGetShaderInfoLog(sh, len, nullptr, buf.get());
      log.assign(buf.get(), static_cast<size_t>(len));
    }
    glDeleteShader(sh);
    throw std::runtime_error("[gl] compile failed: " + log);
  }
  return sh;
}

static GLuint link_program(const GLchar* vert_src, const GLchar* frag_src) {
  GLuint vs = compile_shader(vert_src, GL_VERTEX_SHADER);
  GLuint fs = compile_shader(frag_src, GL_FRAGMENT_SHADER);
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    std::string log;
    if (len > 1) {
      auto buf = std::make_unique<char[]>(static_cast<size_t>(len));
      glGetProgramInfoLog(prog, len, nullptr, buf.get());
      log.assign(buf.get(), static_cast<size_t>(len));
    }
    glDeleteProgram(prog);
    throw std::runtime_error("[gl] link failed: " + log);
  }
  return prog;
}

// ---------------------------------------------------------------------------
// Create / resize the off-screen textures and FBOs.
// Called on first init and whenever the window size changes.
// ---------------------------------------------------------------------------
static void create_framebuffers(int w, int h) {
  // ── ray-trace output texture ─────────────────────────────────────────
  if (ctx.fbo_trace)
    glDeleteFramebuffers(1, &ctx.fbo_trace);
  if (ctx.tex_new)
    glDeleteTextures(1, &ctx.tex_new);

  glGenTextures(1, &ctx.tex_new);
  glBindTexture(GL_TEXTURE_2D, ctx.tex_new);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glGenFramebuffers(1, &ctx.fbo_trace);
  glBindFramebuffer(GL_FRAMEBUFFER, ctx.fbo_trace);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         ctx.tex_new, 0);

  // ── ping-pong accumulation textures ─────────────────────────────────
  glDeleteFramebuffers(2, ctx.fbo_accum);
  glDeleteTextures(2, ctx.tex_accum);

  glGenTextures(2, ctx.tex_accum);
  glGenFramebuffers(2, ctx.fbo_accum);
  for (int i = 0; i < 2; ++i) {
    glBindTexture(GL_TEXTURE_2D, ctx.tex_accum[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx.fbo_accum[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           ctx.tex_accum[i], 0);
    // Clear to black so frame 1 doesn't blend against uninitialized GPU memory.
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  // Reset accumulation ping-pong state.
  ctx.accum_write = 0;
}

// ---------------------------------------------------------------------------
// First-time setup: geometry, shaders, FBOs.
// ---------------------------------------------------------------------------
static void initialize_scene(const Window* window) {
  window->make_current();

  // ── full-screen quad (position + tex-coord) — for trace + display ───
  constexpr float quad_full[] = {
      -1.f, -1.f, 0.f, 0.f, -1.f, 1.f, 0.f, 1.f, 1.f, -1.f, 1.f, 0.f,
      1.f,  -1.f, 1.f, 0.f, -1.f, 1.f, 0.f, 1.f, 1.f, 1.f,  1.f, 1.f,
  };
  glGenVertexArrays(1, &ctx.VAO);
  glGenBuffers(1, &ctx.VBO);
  glBindVertexArray(ctx.VAO);
  glBindBuffer(GL_ARRAY_BUFFER, ctx.VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad_full), quad_full, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void*>(0));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void*>(2 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);

  // ── accumulation quad (position only) ───────────────────────────────
  constexpr float quad_pos[] = {
      -1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f,
  };
  glGenVertexArrays(1, &ctx.quad_VAO);
  glGenBuffers(1, &ctx.quad_VBO);
  glBindVertexArray(ctx.quad_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, ctx.quad_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad_pos), quad_pos, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                        reinterpret_cast<void*>(0));
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);

  // ── render resolution ────────────────────────────────────────────────
  ctx.render_width =
      std::max(1, static_cast<int>(static_cast<float>(window->get_width()) *
                                   config.render_scale));
  ctx.render_height =
      std::max(1, static_cast<int>(static_cast<float>(window->get_height()) *
                                   config.render_scale));

  // ── shaders ──────────────────────────────────────────────────────────
  ctx.prog_trace = link_program(vertex_shader_source, fragment_shader_source);
  ctx.prog_accum = link_program(accum_vertex_source, accum_fragment_source);
  ctx.prog_display =
      link_program(display_vertex_source, display_fragment_source);

  // Cache uniform locations — trace pass
  ctx.loc_iTime = glGetUniformLocation(ctx.prog_trace, "iTime");
  ctx.loc_iResolution = glGetUniformLocation(ctx.prog_trace, "iResolution");
  ctx.loc_iMouse = glGetUniformLocation(ctx.prog_trace, "iMouse");
  ctx.loc_iSamplesPerFrame =
      glGetUniformLocation(ctx.prog_trace, "iSamplesPerFrame");
  ctx.loc_iMaxBounce = glGetUniformLocation(ctx.prog_trace, "iMaxBounce");

  // Cache uniform locations — accumulation pass
  ctx.loc_accum_newFrame = glGetUniformLocation(ctx.prog_accum, "iNewFrame");
  ctx.loc_accum_accumTex = glGetUniformLocation(ctx.prog_accum, "iAccumTex");
  ctx.loc_accum_blendWeight =
      glGetUniformLocation(ctx.prog_accum, "iBlendWeight");

  // Cache uniform locations — display pass
  ctx.loc_display_accumTex =
      glGetUniformLocation(ctx.prog_display, "iAccumTex");

  // Bind texture units statically
  glUseProgram(ctx.prog_accum);
  glUniform1i(ctx.loc_accum_newFrame, 0);  // GL_TEXTURE0
  glUniform1i(ctx.loc_accum_accumTex, 1);  // GL_TEXTURE1

  glUseProgram(ctx.prog_display);
  glUniform1i(ctx.loc_display_accumTex, 0);  // GL_TEXTURE0

  // ── FBOs ─────────────────────────────────────────────────────────────
  create_framebuffers(ctx.render_width, ctx.render_height);

  // Set iResolution once (resize_scene updates it)
  glUseProgram(ctx.prog_trace);
  glUniform2f(ctx.loc_iResolution, static_cast<float>(ctx.render_width),
              static_cast<float>(ctx.render_height));
  glUniform1i(ctx.loc_iSamplesPerFrame, config.samples_per_frame);
  glUniform1i(ctx.loc_iMaxBounce, config.max_bounce);

  ctx.start_time = std::chrono::steady_clock::now();
  ctx.fps_epoch = ctx.start_time;
  ctx.frame_count = 0;
}

static void resize_scene(Window* window) {
  ctx.render_width =
      std::max(1, static_cast<int>(static_cast<float>(window->get_width()) *
                                   config.render_scale));
  ctx.render_height =
      std::max(1, static_cast<int>(static_cast<float>(window->get_height()) *
                                   config.render_scale));

  DLOG_DEBUG("[gl-shadertoy] resize {}x{}", ctx.render_width,
             ctx.render_height);

  create_framebuffers(ctx.render_width, ctx.render_height);

  glUseProgram(ctx.prog_trace);
  glUniform2f(ctx.loc_iResolution, static_cast<float>(ctx.render_width),
              static_cast<float>(ctx.render_height));
}

// ---------------------------------------------------------------------------
// Per-frame callback — three-pass pipeline.
// ---------------------------------------------------------------------------
static void draw_frame(void* userdata, uint32_t /* time */) {
  const auto window = static_cast<Window*>(userdata);
  window->update_buffer_geometry();

  if (!scene_initialized.load(std::memory_order_acquire)) {
    initialize_scene(window);
    scene_initialized.store(true, std::memory_order_release);
  }

  // Detect resize
  const int tw =
      std::max(1, static_cast<int>(static_cast<float>(window->get_width()) *
                                   config.render_scale));
  const int th =
      std::max(1, static_cast<int>(static_cast<float>(window->get_height()) *
                                   config.render_scale));
  if (tw != ctx.render_width || th != ctx.render_height) {
    resize_scene(window);
  }

  const float elapsed = std::chrono::duration<float>(
                            std::chrono::steady_clock::now() - ctx.start_time)
                            .count();

  // ── Pass 1: ray-trace → tex_new ─────────────────────────────────────
  glBindFramebuffer(GL_FRAMEBUFFER, ctx.fbo_trace);
  glViewport(0, 0, ctx.render_width, ctx.render_height);
  glUseProgram(ctx.prog_trace);
  glUniform1f(ctx.loc_iTime, elapsed);
  glBindVertexArray(ctx.VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  // ── Pass 2: accumulate ───────────────────────────────────────────────
  // Read from tex_accum[1-accum_write], write to fbo_accum[accum_write]
  const int accum_read = 1 - ctx.accum_write;
  glBindFramebuffer(GL_FRAMEBUFFER, ctx.fbo_accum[ctx.accum_write]);
  glViewport(0, 0, ctx.render_width, ctx.render_height);
  glUseProgram(ctx.prog_accum);
  // Fixed EMA weight — keeps animated scenes sharp without ghosting.
  glUniform1f(ctx.loc_accum_blendWeight, config.ema_weight);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, ctx.tex_new);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, ctx.tex_accum[accum_read]);
  glBindVertexArray(ctx.quad_VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  // ── Pass 3: display ──────────────────────────────────────────────────
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, window->get_width(), window->get_height());
  glUseProgram(ctx.prog_display);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, ctx.tex_accum[ctx.accum_write]);
  glBindVertexArray(ctx.VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  window->swap_buffers();

  // Advance ping-pong
  ctx.accum_write = accum_read;

  // FPS counter — every 5 s
  ++ctx.frame_count;
  const auto now = std::chrono::steady_clock::now();
  if (const float fps_dt =
          std::chrono::duration<float>(now - ctx.fps_epoch).count();
      fps_dt >= 5.0f) {
    spdlog::info(
        "[gl-shadertoy] {:.1f} FPS  render={}x{}  samples={}  bounces={}",
        static_cast<float>(ctx.frame_count) / fps_dt, ctx.render_width,
        ctx.render_height, config.samples_per_frame, config.max_bounce);
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
      ("w,width",        "Set width",        cxxopts::value<int>()->default_value("250"))
      ("h,height",       "Set height",       cxxopts::value<int>()->default_value("250"))
      ("f,fullscreen",   "Run in fullscreen mode")
      ("m,maximized",    "Run in maximized mode")
      ("r,fullscreen-ratio", "Use fixed width/height ratio when run in fullscreen mode")
      ("t,tearing",      "Enable tearing via the tearing_control protocol")
      ("d,delay",        "Buffer swap delay in microseconds",
                         cxxopts::value<int>()->default_value("0"))
      ("o,opaque",       "Create an opaque surface")
      ("i,interval",     "Set eglSwapInterval",
                         cxxopts::value<int>()->default_value("1"))
      ("b,non-blocking", "eglSwapInterval 0 — removes vsync cap")
      ("s,render-scale", "Render fraction of window resolution (0.1-1.0)",
                         cxxopts::value<float>()->default_value("1.0"))
      ("n,samples",      "Path-trace samples per frame (default 16 = original quality)",
                         cxxopts::value<int>()->default_value("16"))
      ("B,bounces",      "Maximum ray bounce depth (default 32 = original quality)",
                         cxxopts::value<int>()->default_value("32"))
      ("e,ema-weight",   "Temporal EMA blend weight 0.0-1.0 (1.0=no blending, default for animated scenes)",
                         cxxopts::value<float>()->default_value("1.0"));
  // clang-format on
  const auto result = options.parse(argc, argv);

  const float render_scale =
      std::max(0.1f, std::min(1.0f, result["render-scale"].as<float>()));
  const int samples = std::max(1, std::min(64, result["samples"].as<int>()));
  const int bounces = std::max(1, std::min(64, result["bounces"].as<int>()));
  const float ema_weight =
      std::max(0.01f, std::min(1.0f, result["ema-weight"].as<float>()));

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
      .samples_per_frame = samples,
      .max_bounce = bounces,
      .ema_weight = ema_weight,
  };

  spdlog::info(
      "[gl-shadertoy] samples/frame={} bounces={} scale={:.2f} ema={:.2f} "
      "interval={}",
      config.samples_per_frame, config.max_bounce, config.render_scale,
      config.ema_weight, config.interval);

  if (config.opaque) {
    kEglConfigAttribs1[15] = 0;
  }

  try {
    auto [wm_base, wm_type] = WindowManagerFactory::create(display);
    if (wm_type == WindowManagerType::kIvi) {
      throw std::runtime_error("gl-shadertoy: IVI shell is not supported");
    }
    auto wm = std::static_pointer_cast<XdgWindowManager>(wm_base);

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
