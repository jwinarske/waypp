/*
 * Copyright © 2024 Joel Winarske
 * Copyright © 2011 Benjamin Franzke
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
#include <stdexcept>

#include <GLES2/gl2.h>
#include <linux/input.h>
#include <sys/time.h>
#include <cxxopts.hpp>

#include "waypp/window/xdg_toplevel.h"

#include "logging/logging.h"

static std::atomic<bool> running{true};

/// One-shot flag: set to true after the GL scene is initialised on the first
/// draw_frame call. std::atomic ensures visibility across any scheduling
/// boundary without UB, even though draw_frame is always called from the
/// same thread.
static std::atomic<bool> scene_initialized{false};

static constexpr int kResizeMargin = 12;

/// EGL Context Attribute configuration
static constexpr std::array<EGLint, 3> kLocalEglContextAttribs = {{
    // clang-format off
                EGL_CONTEXT_MAJOR_VERSION, 2,
                EGL_NONE
    // clang-format on
}};

/// EGL Configuration Attributes
std::array<EGLint, 13> kLocalEglConfigAttribs = {{
    // clang-format off
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RED_SIZE, 1,
                EGL_GREEN_SIZE, 1,
                EGL_BLUE_SIZE, 1,
                EGL_ALPHA_SIZE, 1,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_NONE
    // clang-format on
}};

struct Configuration {
  int width;
  int height;
  bool fullscreen;
  int maximized;
  bool fullscreen_ratio;
  bool tearing;
  bool toggled_tearing;
  int delay;
  bool opaque;
  int buffer_bpp;
  bool vertical_bar;
  int interval;
} config;

struct {
  GLint rotation_uniform;
  GLuint pos;
  GLuint col;
} gl;

std::shared_ptr<XdgTopLevel> toplevel_;
Seat* seat_{};

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

GLuint load_shader(const GLchar* shaderSrc, const GLenum type) {
  const GLuint shader = glCreateShader(type);
  if (shader == 0)
    return 0;

  glShaderSource(shader, 1, &shaderSrc, nullptr);
  glCompileShader(shader);

  GLint compiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len > 1) {
      auto buf = std::make_unique<char[]>(static_cast<size_t>(len));
      glGetShaderInfoLog(shader, len, nullptr, buf.get());
      std::string res{buf.get(), static_cast<size_t>(len)};
      buf.reset();
      spdlog::error("[gl shader] {}", res.c_str());
      exit(EXIT_FAILURE);
    }
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

void initialize_scene(Window* window) {
  static constexpr GLchar vert_shader_text[] =
      "uniform mat4 rotation;\n"
      "attribute vec4 pos;\n"
      "attribute vec4 color;\n"
      "varying vec4 v_color;\n"
      "void main() {\n"
      "  gl_Position = rotation * pos;\n"
      "  v_color = color;\n"
      "}\n";

  static constexpr GLchar frag_shader_text[] =
      "precision mediump float;\n"
      "varying vec4 v_color;\n"
      "void main() {\n"
      "  gl_FragColor = v_color;\n"
      "}\n";
  window->update_buffer_geometry();

  window->make_current();

  const auto frag = load_shader(frag_shader_text, GL_FRAGMENT_SHADER);
  const auto vert = load_shader(vert_shader_text, GL_VERTEX_SHADER);

  const auto program = glCreateProgram();
  glAttachShader(program, frag);
  glAttachShader(program, vert);
  glLinkProgram(program);

  GLint len = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
  if (len > 1) {
    auto buf = std::make_unique<char[]>(static_cast<size_t>(len));
    glGetProgramInfoLog(program, len, nullptr, buf.get());
    const std::string res{buf.get(), static_cast<size_t>(len)};
    buf.reset();
    spdlog::error("[gl] linking {}", res.c_str());
    exit(EXIT_FAILURE);
  }

  glUseProgram(program);

  gl.pos = 0;
  gl.col = 1;

  glBindAttribLocation(program, gl.pos, "pos");
  glBindAttribLocation(program, gl.col, "color");
  glLinkProgram(program);

  gl.rotation_uniform = glGetUniformLocation(program, "rotation");
}

enum weston_matrix_transform_type {
  WESTON_MATRIX_TRANSFORM_SCALE = (1 << 1),
  WESTON_MATRIX_TRANSFORM_ROTATE = (1 << 2),
};

struct weston_matrix {
  float d[16];
  unsigned int type;
};

/*
 * Matrices are stored in column-major order, that is the array indices are:
 *  0  4  8 12
 *  1  5  9 13
 *  2  6 10 14
 *  3  7 11 15
 */

void weston_matrix_init(weston_matrix* matrix) {
  static constexpr weston_matrix identity = {
      .d = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
      .type = 0,
  };

  memcpy(matrix, &identity, sizeof identity);
}

/* m <- n * m, that is, m is multiplied on the LEFT. */
void weston_matrix_multiply(weston_matrix* m, const weston_matrix* n) {
  weston_matrix tmp{};

  for (auto i = 0; i < 4; i++) {
    const auto row = m->d + i * 4;
    for (auto j = 0; j < 4; j++) {
      tmp.d[4 * i + j] = 0;
      const auto column = n->d + j;
      for (auto k = 0; k < 4; k++)
        tmp.d[4 * i + j] += row[k] * column[k * 4];
    }
  }
  tmp.type = m->type | n->type;
  memcpy(m, &tmp, sizeof tmp);
}

void weston_matrix_scale(weston_matrix* matrix, float x, float y, float z) {
  const weston_matrix scale = {
      .d = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1},
      .type = WESTON_MATRIX_TRANSFORM_SCALE,
  };

  weston_matrix_multiply(matrix, &scale);
}

void weston_matrix_rotate_xy(weston_matrix* matrix, float cos, float sin) {
  const weston_matrix translate = {
      .d = {cos, sin, 0, 0, -sin, cos, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
      .type = WESTON_MATRIX_TRANSFORM_ROTATE,
  };

  weston_matrix_multiply(matrix, &translate);
}

uint32_t frames;
uint32_t initial_frame_time;
uint32_t benchmark_time;

static void draw_triangle(Window* window, EGLint buffer_age) {
  static constexpr GLfloat verts[3][2] = {{-0.5, -0.5}, {0.5, -0.5}, {0, 0.5}};
  static constexpr GLfloat colors[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

  glVertexAttribPointer(gl.pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
  glVertexAttribPointer(gl.col, 3, GL_FLOAT, GL_FALSE, 0, colors);
  glEnableVertexAttribArray(gl.pos);
  glEnableVertexAttribArray(gl.col);

  glDrawArrays(GL_TRIANGLES, 0, 3);

  glDisableVertexAttribArray(gl.pos);
  glDisableVertexAttribArray(gl.col);

  usleep(static_cast<__useconds_t>(config.delay));

  if (config.opaque || config.fullscreen) {
    window->opaque_region_add(0, 0, window->get_max_width(),
                              window->get_max_height());
  } else {
    window->opaque_region_clear();
  }

  if (window->have_swap_buffers_width_damage() && buffer_age > 0) {
    EGLint rect[4] = {window->get_width() / 4 - 1, window->get_height() / 4 - 1,
                      window->get_width() / 2 + 2,
                      window->get_height() / 2 + 2};
    window->swap_buffers_with_damage(rect, 1);
  } else {
    window->swap_buffers();
  }
}

/**
 * @brief Updates the frame by drawing it.
 *
 * This function updates the frame by drawing it on the screen. It sets the
 * OpenGL clear color based on the calculated hue, clears the color buffer,
 * swaps the buffers to display the updated frame, and clears the current
 * rendering context.
 *
 * @param userdata A pointer to the WindowEgl object.
 * @param time The current time in milliseconds.
 */
static void draw_frame(void* userdata, uint32_t /* time */) {
  const auto window = static_cast<Window*>(userdata);

  if (!scene_initialized.load(std::memory_order_acquire)) {
    initialize_scene(window);
    scene_initialized.store(true, std::memory_order_release);
  }

  GLfloat angle;
  static constexpr uint32_t speed_div = 5, benchmark_interval = 5;

  window->update_buffer_geometry();

  timeval tv{};
  gettimeofday(&tv, nullptr);
  const auto time = static_cast<uint32_t>(tv.tv_sec * 1000 + tv.tv_usec / 1000);
  if (frames == 0) {
    initial_frame_time = time;
    benchmark_time = time;
  }
  if (time - benchmark_time > (benchmark_interval * 1000)) {
    printf("%d frames in %d seconds: %f fps\n", frames, benchmark_interval,
           static_cast<float>(frames) / benchmark_interval);
    benchmark_time = time;
    frames = 0;
  }

  if (config.vertical_bar) {
    angle = 0;
  } else {
    angle = static_cast<GLfloat>(((time - initial_frame_time) / speed_div) %
                                 360 * M_PI / 180.0);
  }
  weston_matrix rotation{};
  weston_matrix_init(&rotation);
  rotation.d[0] = cos(angle);
  rotation.d[2] = sin(angle);
  rotation.d[8] = -sin(angle);
  rotation.d[10] = cos(angle);

  switch (window->get_buffer_transform()) {
    case WL_OUTPUT_TRANSFORM_FLIPPED:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      weston_matrix_scale(&rotation, -1, 1, 1);
      break;
    default:
      break;
  }

  switch (window->get_buffer_transform()) {
    default:
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_FLIPPED:
      break;
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
      weston_matrix_rotate_xy(&rotation, 0, 1);
      break;
    case WL_OUTPUT_TRANSFORM_180:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
      weston_matrix_rotate_xy(&rotation, -1, 0);
      break;
    case WL_OUTPUT_TRANSFORM_270:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      weston_matrix_rotate_xy(&rotation, 0, -1);
      break;
  }

  EGLint buffer_age = 0;
  if (window->have_swap_buffers_width_damage())
    window->get_buffer_age(buffer_age);

  glViewport(0, 0, window->get_width(), window->get_height());

  glUniformMatrix4fv(gl.rotation_uniform, 1, GL_FALSE, (GLfloat*)rotation.d);

  if (config.opaque || config.fullscreen)
    glClearColor(0.0, 0.0, 0.0, 1);
  else
    glClearColor(0.0, 0.0, 0.0, 0.5);
  glClear(GL_COLOR_BUFFER_BIT);

  draw_triangle(window, buffer_age);

  frames++;
}

class Observer final : public SeatObserver,
                       public KeyboardObserver,
                       public PointerObserver {
 public:
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

  void notify_pointer_enter(Pointer* /* pointer */,
                            wl_pointer* /* pointer */,
                            uint32_t serial,
                            wl_surface* surface,
                            double sx,
                            double sy) override {
    spdlog::info("Pointer Enter: serial: {}, surface: {}, x: {}, y: {}", serial,
                 fmt::ptr(surface), sx, sy);
  }

  void notify_pointer_leave(Pointer* /* pointer */,
                            wl_pointer* /* pointer */,
                            uint32_t serial,
                            wl_surface* surface) override {
    spdlog::info("Pointer Leave: serial: {}, surface: {}", serial,
                 fmt::ptr(surface));
  }

  void notify_pointer_motion(Pointer* /* pointer  */,
                             wl_pointer* /* pointer */,
                             uint32_t time,
                             double sx,
                             double sy) override {
    spdlog::info("Pointer: time: {}, x: {}, y: {}", time, sx, sy);
    if (toplevel_->is_resizing()) {
      spdlog::info("Resizing: x: {}, y: {}", sx, sy);
    }
  }

  void notify_pointer_button(Pointer* pointer,
                             wl_pointer* /* pointer  */,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t button,
                             uint32_t state) override {
    spdlog::info("Pointer Button: pointer: {}, time: {}, button: {}, state: {}",
                 serial, time, button, state);
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
      auto edge = toplevel_->check_edge_resize(pointer->get_xy());
      if (edge != XDG_TOPLEVEL_RESIZE_EDGE_NONE) {
        toplevel_->resize(seat_->get_seat(), serial, edge);
      }
    }
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
  };

  void notify_pointer_axis_source(Pointer* /* pointer */,
                                  wl_pointer* /* pointer */,
                                  uint32_t axis_source) override {
    spdlog::info("Pointer Axis Source: axis_source: {}", axis_source);
  };

  void notify_pointer_axis_stop(Pointer* /* pointer */,
                                wl_pointer* /* pointer */,
                                uint32_t /* time */,
                                uint32_t axis) override {
    spdlog::info("Pointer Axis Stop: axis: {}", axis);
  };

  void notify_pointer_axis_discrete(Pointer* /* pointer */,
                                    wl_pointer* /*pointer */,
                                    uint32_t axis,
                                    int32_t discrete) override {
    spdlog::info("Pointer Axis Discrete: axis: {}, discrete: {}", axis,
                 discrete);
  }
};

/**
 * @brief Main function for the program.
 *
 * This function initializes the surface manager and creates a surface with the
 * specified dimensions and type. It sets up a signal handler for SIGINT
 * (Ctrl+C) to stop the program, and then enters a loop to handle surface
 * events.
 *
 * @param argc The number of command line arguments.
 * @param argv An array of strings representing the command line arguments.
 * @return An integer representing the exit status of the program.
 */
int main(const int argc, char** argv) {
  auto logging = std::make_unique<Logging>();

  auto display = wl_display_connect(nullptr);
  if (!display) {
    spdlog::critical("Unable to connect to Wayland display socket.");
    return EXIT_FAILURE;
  }

  std::signal(SIGINT, handle_signal);

  cxxopts::Options options("simple-egl", "Weston simple-egl example");
  options.add_options()
      // clang-format off
            ("w,width", "Set width", cxxopts::value<int>()->default_value("250"))
            ("h,height", "Set height", cxxopts::value<int>()->default_value("250"))
            ("f,fullscreen", "Run in fullscreen mode")
            ("m,maximized", "Run in maximized mode")
            ("r,fullscreen-ratio", "Use fixed width/height ratio when run in fullscreen mode")
            ("t,tearing", "Enable tearing via the tearing_control protocol")
            ("d,delay", "Buffer swap delay in microseconds", cxxopts::value<int>()->default_value("0"))
            ("o,opaque", "Create an opaque surface")
            ("s,buffer-bpp", "Use a 16 bpp EGL config")
            ("v,vertical-bar", "Draw a moving vertical bar instead of a triangle")
            ("i,interval", "Set eglSwapInterval to interval", cxxopts::value<int>()->default_value("1"))
            ("b,non-blocking", "Don't sync to compositor redraw (eglSwapInterval 0)");
  // clang-format on
  const auto result = options.parse(argc, argv);

  config = {
      .width = result["width"].as<int>(),
      .height = result["height"].as<int>(),
      .fullscreen = result["fullscreen"].as<bool>(),
      .maximized = result["maximized"].as<bool>(),
      .fullscreen_ratio = result["fullscreen-ratio"].as<bool>(),
      .tearing = result["tearing"].as<bool>(),
      .toggled_tearing = false,
      .delay = result["delay"].as<int>(),
      .opaque = result["opaque"].as<bool>(),
      .buffer_bpp = result["buffer-bpp"].as<bool>() ? 16 : 0,
      .vertical_bar = result["vertical-bar"].as<bool>(),
      .interval = result["interval"].as<int>(),
  };

  if (result["tearing"].as<bool>()) {
    config.tearing = true;
    config.toggled_tearing = true;
  }

  if (result["non-blocking"].as<bool>()) {
    config.interval = 0;
  }

  /// Control EGL_ALPHA_SIZE value
  if (config.opaque || config.buffer_bpp == 16) {
    kLocalEglConfigAttribs[9] = 0;
  }

  try {
    auto wm = std::make_shared<XdgWindowManager>(display);
    const auto observer = std::make_unique<Observer>();
    if (wm->get_seat().has_value()) {
      seat_ = wm->get_seat().value();
      seat_->register_observer(observer.get());
    }

    Egl::config egl_config{};
    egl_config.context_attribs_size = kLocalEglContextAttribs.size();
    egl_config.context_attribs = kLocalEglContextAttribs.data();
    egl_config.config_attribs_size = kLocalEglConfigAttribs.size();
    egl_config.config_attribs = kLocalEglConfigAttribs.data();
    egl_config.buffer_bpp = config.buffer_bpp;
    egl_config.swap_interval = config.interval;
    egl_config.type = Egl::OPENGL_ES_API;

    toplevel_ = wm->create_top_level(
        "simple-egl", "org.freedesktop.gitlab.jwinarske.waypp.simple_egl",
        config.width, config.height, kResizeMargin, 0, 0, config.fullscreen,
        config.maximized, config.fullscreen_ratio, config.tearing, draw_frame,
        &egl_config);

    toplevel_->start_frame_callbacks();

    while (running.load(std::memory_order_acquire) && toplevel_->is_valid() && wm->display_dispatch() != -1) {
    }

    toplevel_.reset();
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
