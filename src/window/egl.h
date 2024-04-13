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

#pragma once

#include <cstddef>
#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

class SurfaceEgl;

class Egl {
public:
    explicit Egl(struct wl_display *display, struct wl_surface *wl_surface, int width, int height,
                 const int32_t *context_attribs, size_t context_attribs_size,
                 const int32_t *config_attribs, size_t config_attribs_size, int buffer_bpp);

    ~Egl();

    void set_swap_interval(int interval);


    void make_current();

    void clear_current();

    void swap_buffers();

    void resize(int width, int height, int dx, int dy);

    // Disallow copy and assign.
    Egl(const Egl &) = delete;

    Egl &operator=(const Egl &) = delete;

private:
    friend class SurfaceEgl;

    std::vector<EGLint> context_attribs_;
    std::vector<EGLint> config_attribs_;
    int buffer_bpp_;

    EGLContext context_{};

    EGLint major_{}, minor_{};

    EGLDisplay dpy_;
    EGLConfig config_{};

    struct wl_surface *wl_surface_;
    struct wl_egl_window *wl_egl_window_{};
    EGLSurface egl_surface_{};

    int width_;
    int height_;

    PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC pfSwapBufferWithDamage_{};
    PFNEGLSETDAMAGEREGIONKHRPROC pfSetDamageRegion_{};

    static bool has_egl_extension(const char *extensions, const char *name);

    static void debug_callback(EGLenum error,
                               const char *command,
                               EGLint messageType,
                               EGLLabelKHR threadLabel,
                               EGLLabelKHR objectLabel,
                               const char *message);

    static void egl_khr_debug_init();
};
