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

#include <csignal>

#include <GLES2/gl2.h>
#include <cxxopts.hpp>
#include <sys/time.h>

#include "window/xdg_toplevel.h"

#include "logging.h"

static volatile bool running = true;

volatile bool scene_initialized = false;

/// EGL Context Attribute configuration
static constexpr std::array<EGLint, 3> kEglContextAttribs = {
        {
                EGL_CONTEXT_MAJOR_VERSION, 2,
                EGL_NONE
        }
};

/// EGL Configuration Attributes
std::array<EGLint, 13> kEglConfigAttribs = {
        {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RED_SIZE, 1,
                EGL_GREEN_SIZE, 1,
                EGL_BLUE_SIZE, 1,
                EGL_ALPHA_SIZE, 1,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_NONE
        }
};

typedef struct {
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
} CONFIGURATION_T;

CONFIGURATION_T config;

struct {
    GLint rotation_uniform;
    GLuint pos;
    GLuint col;
} gl;

/**
 * @brief Signal handler function to handle signals.
 *
 * This function is a signal handler for handling signals. It sets the value of keep_running
 * to false, which will stop the program from running. The function does not take any input
 * parameters.
 *
 * @param signal The signal number. This parameter is not used by the function.
 *
 * @return void
 */
void handle_signal(int signal) {
    if (signal == SIGINT) {
        running = false;
    }
}

GLuint load_shader(const GLchar *shaderSrc, const GLenum type) {
    // Create the shader object
    const GLuint shader = glCreateShader(type);
    if (shader == 0)
        return 0;
    glShaderSource(shader, 1, &shaderSrc, nullptr);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            auto *infoLog = static_cast<GLchar *>(
                    malloc(sizeof(char) * static_cast<unsigned long>(infoLen)));
            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
            spdlog::error("Error compiling shader:\n{}", infoLog);
            free(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void initialize_scene(Window *window) {
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

    auto frag = load_shader(frag_shader_text, GL_FRAGMENT_SHADER);
    auto vert = load_shader(vert_shader_text, GL_VERTEX_SHADER);

    auto program = glCreateProgram();
    glAttachShader(program, frag);
    glAttachShader(program, vert);
    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(program, 1000, &len, log);
        fprintf(stderr, "Error: linking:\n%.*s\n", len, log);
        exit(1);
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

void weston_matrix_init(struct weston_matrix *matrix) {
    static const struct weston_matrix identity = {
            .d = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
            .type = 0,
    };

    memcpy(matrix, &identity, sizeof identity);
}

/* m <- n * m, that is, m is multiplied on the LEFT. */
void weston_matrix_multiply(struct weston_matrix *m, const struct weston_matrix *n) {
    struct weston_matrix tmp{};
    const float *row, *column;
    int i, j, k;

    for (i = 0; i < 4; i++) {
        row = m->d + i * 4;
        for (j = 0; j < 4; j++) {
            tmp.d[4 * i + j] = 0;
            column = n->d + j;
            for (k = 0; k < 4; k++)
                tmp.d[4 * i + j] += row[k] * column[k * 4];
        }
    }
    tmp.type = m->type | n->type;
    memcpy(m, &tmp, sizeof tmp);
}

void weston_matrix_scale(struct weston_matrix *matrix, float x, float y, float z) {
    struct weston_matrix scale = {
            .d = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1},
            .type = WESTON_MATRIX_TRANSFORM_SCALE,
    };

    weston_matrix_multiply(matrix, &scale);
}

void weston_matrix_rotate_xy(struct weston_matrix *matrix, float cos, float sin) {
    struct weston_matrix translate = {
            .d = {cos, sin, 0, 0, -sin, cos, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
            .type = WESTON_MATRIX_TRANSFORM_ROTATE,
    };

    weston_matrix_multiply(matrix, &translate);
}

uint32_t frames;
uint32_t initial_frame_time;
uint32_t benchmark_time;

static void draw_triangle(Window *window) {
    static const GLfloat verts[3][2] = {
            {-0.5, -0.5},
            {0.5,  -0.5},
            {0,    0.5}
    };
    static const GLfloat colors[3][3] = {
            {1, 0, 0},
            {0, 1, 0},
            {0, 0, 1}
    };

    glVertexAttribPointer(gl.pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glVertexAttribPointer(gl.col, 3, GL_FLOAT, GL_FALSE, 0, colors);
    glEnableVertexAttribArray(gl.pos);
    glEnableVertexAttribArray(gl.col);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(gl.pos);
    glDisableVertexAttribArray(gl.col);

    usleep(static_cast<__useconds_t>(config.delay));

#if 0
    struct wl_region *region;
    if (config.opaque || config.fullscreen) {
        region = wl_compositor_create_region(window->display->compositor);
        wl_region_add(region, 0, 0, INT32_MAX, INT32_MAX);
        wl_surface_set_opaque_region(window->surface, region);
        wl_region_destroy(region);
    } else {
        wl_surface_set_opaque_region(window->surface, NULL);
    }

    EGLint rect[4];
    if (display->swap_buffers_with_damage && buffer_age > 0) {
        rect[0] = window->buffer_size.width / 4 - 1;
        rect[1] = window->buffer_size.height / 4 - 1;
        rect[2] = window->buffer_size.width / 2 + 2;
        rect[3] = window->buffer_size.height / 2 + 2;
        display->swap_buffers_with_damage(display->egl.dpy,
                                          window->egl_surface,
                                          rect, 1);
    } else {
#endif
    window->swap_buffers();
//TODO    }
}

/**
 * @brief Updates the frame by drawing it.
 *
 * This function updates the frame by drawing it on the screen. It sets the OpenGL clear color based on the calculated hue,
 * clears the color buffer, swaps the buffers to display the updated frame, and clears the current rendering context.
 *
 * @param data A pointer to the WindowEgl object.
 * @param time The current time in milliseconds.
 */
static void draw_frame(void *userdata, uint32_t /* time */) {
    auto window = static_cast<Window *>(userdata);

    if (!scene_initialized) {
        initialize_scene(window);
        scene_initialized = true;
    }

    GLfloat angle;
    static const uint32_t speed_div = 5, benchmark_interval = 5;

    window->update_buffer_geometry();

    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    auto time = static_cast<uint32_t>(tv.tv_sec * 1000 + tv.tv_usec / 1000);
    if (frames == 0) {
        initial_frame_time = time;
        benchmark_time = time;
    }
    if (time - benchmark_time > (benchmark_interval * 1000)) {
        printf("%d frames in %d seconds: %f fps\n",
               frames,
               benchmark_interval,
               (float) frames / benchmark_interval);
        benchmark_time = time;
        frames = 0;
    }

    if (config.vertical_bar) {
        angle = 0;
    } else {
        angle = static_cast<GLfloat>(((time - initial_frame_time) / speed_div)
                                     % 360 * M_PI / 180.0);
    }
    struct weston_matrix rotation{};
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

    glViewport(0, 0, window->get_width(), window->get_height());

    glUniformMatrix4fv(gl.rotation_uniform, 1, GL_FALSE, (GLfloat *) rotation.d);

    if (config.opaque || config.fullscreen)
        glClearColor(0.0, 0.0, 0.0, 1);
    else
        glClearColor(0.0, 0.0, 0.0, 0.5);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_triangle(window);

    frames++;
}

/**
 * @brief Main function for the program.
 *
 * This function initializes the surface manager and creates a surface with the specified dimensions and type.
 * It sets up a signal handler for SIGINT (Ctrl+C) to stop the program, and then enters a loop to handle surface events.
 *
 * @param argc The number of command line arguments.
 * @param argv An array of strings representing the command line arguments.
 * @return An integer representing the exit status of the program.
 */
int main(int argc, char **argv) {

    auto logging = std::make_unique<Logging>();

    std::signal(SIGINT, handle_signal);

    cxxopts::Options options("simple-egl", "Weston simple-egl example");
    options.add_options()
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
    auto result = options.parse(argc, argv);

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

    // Control EGL_ALPHA_SIZE value
    if (config.opaque || config.buffer_bpp == 16) {
        kEglConfigAttribs[9] = 0;
    }

    XdgWindowManager wm;
    auto top_level = wm.create_top_level("simple-egl",
                                         config.width,
                                         config.height,
                                         0,
                                         0,
                                         config.fullscreen,
                                         config.maximized,
                                         config.fullscreen_ratio,
                                         config.tearing,
                                         draw_frame,
                                         kEglContextAttribs.data(), kEglContextAttribs.size(),
                                         kEglConfigAttribs.data(), kEglConfigAttribs.size(),
                                         config.buffer_bpp, config.interval);

    top_level->update_buffer_geometry();
    top_level->start_frame_callbacks();

    while (running && top_level->is_valid() && wm.display_dispatch() != -1) {}

    top_level->stop_frame_callbacks();

    return EXIT_SUCCESS;
}
