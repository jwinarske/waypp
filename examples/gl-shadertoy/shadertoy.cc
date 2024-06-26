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

#include <chrono>
#include <csignal>

#include <GLES3/gl32.h>
#include <cxxopts.hpp>
#include <glm/glm.hpp>

#include "logging/logging.h"
#include "shaders/glsl-ray-tracing-shaders.h"
#include "waypp/window/xdg_toplevel.h"

class App;

static volatile bool running = true;

volatile bool scene_initialized = false;

static constexpr int kResizeMargin = 12;

/// EGL Context Attribute configuration
std::array<EGLint, 7> kEglContextAttribs1 = {
        {
                // clang-format off
                EGL_CONTEXT_MAJOR_VERSION, 3,
                EGL_CONTEXT_MINOR_VERSION, 3,
                EGL_CONTEXT_OPENGL_PROFILE_MASK,
                EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                EGL_NONE,
                // clang-format on
        }
};

/// EGL Configuration Attributes
std::array<EGLint, 21> kEglConfigAttribs1 = {
        {
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
        }
};

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
} config;

struct Context {
    GLuint framebuffer;
    GLuint shader_program;
    GLuint VAO;
} ctx;

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

GLuint load_shader(const GLchar *shader_source, const GLenum shader_type) {
    const GLuint shader = glCreateShader(shader_type);
    if (shader == 0)
        return 0;

    glShaderSource(shader, 1, &shader_source, nullptr);
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

void initialize_scene(Window *window) {
    /// Quad

    float quad_vertices[] = {
            -1.0, -1.0, 0.0, 0.0, -1.0, 1.0, 0.0, 1.0, 1.0, -1.0, 1.0, 0.0,

            1.0, -1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0};

    window->make_current();

    glGenVertexArrays(1, &ctx.VAO);
    glBindVertexArray(ctx.VAO);

    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    /// Framebuffer

    glGenFramebuffers(1, &ctx.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx.framebuffer);

    /// Texture

    GLuint texColor;
    glGenTextures(1, &texColor);
    glBindTexture(GL_TEXTURE_2D, texColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, window->get_width(),
                 window->get_height(), 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texColor, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /// Shaders

    auto vertex_shader = load_shader(vertex_shader_source, GL_VERTEX_SHADER);
    if (!vertex_shader) {
        spdlog::error("Failed to load Vertex shader");
        exit(EXIT_FAILURE);
    }

    auto fragment_shader =
            load_shader(fragment_shader_source, GL_FRAGMENT_SHADER);
    if (!fragment_shader) {
        spdlog::error("Failed to load Fragment shader");
        exit(EXIT_FAILURE);
    }

    ctx.shader_program = glCreateProgram();
    glAttachShader(ctx.shader_program, vertex_shader);
    glAttachShader(ctx.shader_program, fragment_shader);
    glLinkProgram(ctx.shader_program);

    GLint len = 0;
    glGetProgramiv(ctx.shader_program, GL_INFO_LOG_LENGTH, &len);
    if (len > 1) {
        auto buf = std::make_unique<char[]>(static_cast<size_t>(len));
        glGetProgramInfoLog(ctx.shader_program, len, nullptr, buf.get());
        std::string res{buf.get(), static_cast<size_t>(len)};
        buf.reset();
        spdlog::error("[gl] linking {}", res.c_str());
        exit(EXIT_FAILURE);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glUseProgram(ctx.shader_program);

    glm::vec2 screen(window->get_width(), window->get_height());
    glUniform2fv(glGetUniformLocation(ctx.shader_program, "iResolution"), 1,
                 &screen[0]);
}

/**
 * @brief Updates the frame by drawing it.
 *
 * This function updates the frame by drawing it on the screen. It sets the
 * OpenGL clear color based on the calculated hue, clears the color buffer,
 * swaps the buffers to display the updated frame, and clears the current
 * rendering context.
 *
 * @param data A pointer to the WindowEgl object.
 * @param time The current time in milliseconds.
 */
static void draw_frame(void *userdata, uint32_t /* time */) {
    auto window = static_cast<Window *>(userdata);

    window->update_buffer_geometry();

    if (!scene_initialized) {
        initialize_scene(window);
        scene_initialized = true;
    }

    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
    const auto current_frame = std::chrono::duration<float>(now).count();

    glBindFramebuffer(GL_FRAMEBUFFER, ctx.framebuffer);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(ctx.shader_program);

    glUniform1f(glGetUniformLocation(ctx.shader_program, "iTime"),
                static_cast<float>(static_cast<int>(current_frame) % 60));
    glBindVertexArray(ctx.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    window->swap_buffers();
}

class EventObserver : public SeatObserver, public KeyboardObserver {
public:
    void notify_seat_capabilities(Seat *seat,
                                  wl_seat * /* seat */,
                                  uint32_t /* caps */) override {
        if (seat) {
            auto keyboard = seat->get_keyboard();
            if (keyboard.has_value()) {
                keyboard.value()->register_observer(this);
            }
        }
    }

    void notify_seat_name(Seat * /* seat */,
                          wl_seat * /* seat */,
                          const char *name) override {
        spdlog::info("Seat: {}", name);
    }

    void notify_keyboard_enter(Keyboard * /* keyboard */,
                               wl_keyboard * /* wl_keyboard */,
                               uint32_t serial,
                               wl_surface *surface,
                               wl_array * /* keys */) override {
        spdlog::info("Keyboard Enter: serial: {}, surface: {}", serial,
                     fmt::ptr(surface));
    }

    void notify_keyboard_leave(Keyboard * /* keyboard */,
                               wl_keyboard * /* wl_keyboard */,
                               uint32_t serial,
                               wl_surface *surface) override {
        spdlog::info("Keyboard Leave: serial: {}, surface: {}", serial,
                     fmt::ptr(surface));
    }

    void notify_keyboard_keymap(Keyboard * /* keyboard */,
                                wl_keyboard * /* wl_keyboard */,
                                uint32_t format,
                                int32_t fd,
                                uint32_t size) override {
        spdlog::info("Keymap: format: {}, fd: {}, size: {}", format, fd, size);
    }

    void notify_keyboard_xkb_v1_key(
            Keyboard * /* keyboard */,
            wl_keyboard * /* wl_keyboard */,
            uint32_t serial,
            uint32_t time,
            uint32_t xkb_scancode,
            bool key_repeats,
            uint32_t state,
            int xdg_key_symbol_count,
            const xkb_keysym_t *xdg_key_symbols) override {
        spdlog::info(
                "Key: serial: {}, time: {}, xkb_scancode: 0x{:X}, key_repeats: {}, "
                "state: {}, xdg_keysym_count: {}, syms_out[0]: 0x{:X}",
                serial, time, xkb_scancode, key_repeats,
                state == KeyState::KEY_STATE_PRESS ? "press" : "release",
                xdg_key_symbol_count, xdg_key_symbols[0]);
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
int main(int argc, char **argv) {
    auto logging = std::make_unique<Logging>();

    auto display = wl_display_connect(nullptr);
    if (!display) {
        spdlog::critical("Unable to connect to Wayland socket.");
        exit(EXIT_FAILURE);
    }

    std::signal(SIGINT, handle_signal);

    cxxopts::Options options("gl-shadertoy", "OpenGL Shadertoy");
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
            ("i,interval", "Set eglSwapInterval to interval", cxxopts::value<int>()->default_value("1"))
            ("b,non-blocking", "Don't sync to compositor redraw (eglSwapInterval 0)");

    // clang-format on
    auto result = options.parse(argc, argv);

    config = {
            .width = result["width"].as<int>(),
            .height = result["height"].as<int>(),
            .fullscreen = result["fullscreen"].as<bool>(),
            .maximized = result["maximized"].as<bool>(),
            .fullscreen_ratio = result["fullscreen-ratio"].as<bool>(),
            .tearing = result["tearing"].as<bool>(),
            .delay = result["delay"].as<int>(),
            .opaque = result["opaque"].as<bool>(),
            .interval = result["non-blocking"].as<bool>() ? 0 : result["interval"].as<int>(),
    };

    /// Control EGL_ALPHA_SIZE value
    if (config.opaque) {
        kEglConfigAttribs1[15] = 0;
    }

    auto wm = std::make_shared<XdgWindowManager>(display);
    auto event_observer = std::make_unique<EventObserver>();
    auto seat = wm->get_seat();
    if (seat.has_value()) {
        seat.value()->register_observer(event_observer.get());
    }

    Egl::config egl_config{};
    egl_config.context_attribs_size = kEglContextAttribs1.size();
    egl_config.context_attribs = kEglContextAttribs1.data();
    egl_config.config_attribs_size = kEglConfigAttribs1.size();
    egl_config.config_attribs = kEglConfigAttribs1.data();
    egl_config.buffer_bpp = 32;
    egl_config.swap_interval = config.interval;
    egl_config.type = Egl::OPENGL_API;

    auto top_level = wm->create_top_level(
            "simple-egl", "org.freedesktop.gitlab.jwinarske.waypp.gl-shadertoy",
            config.width, config.height, kResizeMargin, 0, 0, config.fullscreen, config.maximized,
            config.fullscreen_ratio, config.tearing, draw_frame, &egl_config);

    top_level->start_frame_callbacks();

    while (running && top_level->is_valid() && wm->display_dispatch() != -1) {
    }

    top_level.reset();
    wm.reset();
    wl_display_flush(display);
    wl_display_disconnect(display);

    return EXIT_SUCCESS;
}
