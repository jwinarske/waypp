/*
 * Copyright 2024 Joel Winarske
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_WINDOW_EGL_H_
#define SRC_WINDOW_EGL_H_

#include <array>

#include <EGL/egl.h>
#include <EGL/eglext.h>

class Egl {
public:
    explicit Egl(struct wl_display *display);

    ~Egl();

    [[nodiscard]] bool clear_current() const;

    [[nodiscard]] bool make_current() const;

    [[nodiscard]] bool swap_buffers() const;

    [[nodiscard]] bool make_resource_current() const;

    [[nodiscard]] bool make_texture_current() const;

    [[nodiscard]] PFNEGLSETDAMAGEREGIONKHRPROC get_set_damage_region() const {
        return pfSetDamageRegion_;
    }

    [[nodiscard]] PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC
    get_swap_buffers_with_damage() const {
        return pfSwapBufferWithDamage_;
    }

    [[maybe_unused]] [[nodiscard]] bool has_ext_buffer_age() const { return has_egl_ext_buffer_age_; }

    [[maybe_unused]] EGLDisplay get_display() { return dpy_; }

    [[maybe_unused]] EGLContext get_texture_context() { return texture_context_; }

    friend class WindowEgl;

private:
    static constexpr std::array<EGLint, 5> kEglContextAttribs = {
            {
                    EGL_CONTEXT_MAJOR_VERSION, 3,
                    EGL_CONTEXT_MAJOR_VERSION, 2,
                    EGL_NONE
            }
    };

    static constexpr std::array<EGLint, 27> kEglConfigAttribs = {
            {
                    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_RED_SIZE, 8,
                    EGL_GREEN_SIZE, 8,
                    EGL_BLUE_SIZE, 8,
                    EGL_ALPHA_SIZE, 8,
                    EGL_STENCIL_SIZE, 8,
                    EGL_DEPTH_SIZE, 16,
                    EGL_SAMPLE_BUFFERS, 1,
                    EGL_SAMPLES, 4,
                    EGL_NONE // termination sentinel
            }
    };

    EGLSurface egl_surface_{};
    EGLConfig config_{};
    EGLContext texture_context_{};

    int buffer_size_ = 24;

    EGLDisplay dpy_{};
    EGLContext context_{};
    EGLContext resource_context_{};

    EGLint major_{};
    EGLint minor_{};

    PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC pfSwapBufferWithDamage_{};
    PFNEGLSETDAMAGEREGIONKHRPROC pfSetDamageRegion_{};
    bool has_egl_ext_buffer_age_{};

    static bool has_egl_extension(const char *extensions, const char *name);

    static void debug_callback(EGLenum error,
                               const char *command,
                               EGLint messageType,
                               EGLLabelKHR threadLabel,
                               EGLLabelKHR objectLabel,
                               const char *message);

    static void egl_khr_debug_init();
};

#endif // SRC_WINDOW_EGL_H_