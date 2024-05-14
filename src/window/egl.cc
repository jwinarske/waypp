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

#include "egl.h"

#include <wayland-egl.h>
#include <cstring>

#include "logging.h"

/**
 * @brief The Egl class represents an EGL object used for OpenGL rendering.
 *
 * This class provides functionality for initializing EGL, choosing an EGL
 * configuration, creating an EGL context, and managing various EGL extensions.
 */
Egl::Egl(struct wl_display *display,
         struct wl_surface *wl_surface,
         int width,
         int height,
         const int32_t *context_attribs,
         size_t context_attribs_size,
         const int32_t *config_attribs,
         size_t config_attribs_size,
         int buffer_bpp,
         enum api type)
        : dpy_(eglGetDisplay(display)),
          context_attribs_(context_attribs, context_attribs + context_attribs_size),
          config_attribs_(config_attribs, config_attribs + config_attribs_size),
          buffer_bpp_(buffer_bpp),
          wl_surface_(wl_surface),
          width_(width),
          height_(height) {
    SPDLOG_TRACE("++Egl::Egl()");
    EGLBoolean ret = eglInitialize(dpy_, &major_, &minor_);
    if (ret == EGL_FALSE) {
        throw std::runtime_error("eglInitialize failed.");
    }

    ret = eglBindAPI(type);
    if (ret != EGL_TRUE) {
        throw std::runtime_error("eglBindAPI failed.");
    }

    EGLint count;
    eglGetConfigs(dpy_, nullptr, 0, &count);
    SPDLOG_DEBUG("EGL has {} configs", count);

    auto *configs = reinterpret_cast<EGLConfig *>(
            calloc(static_cast<size_t>(count), sizeof(EGLConfig)));

    EGLint n;
    ret = eglChooseConfig(dpy_, config_attribs_.data(), configs, count, &n);
    if (n == 0) {
        SPDLOG_DEBUG("EGL Config: Check Config Attributes");
        exit(EXIT_FAILURE);
    }

    EGLint red_size;
    for (EGLint i = 0; i < n; i++) {
        eglGetConfigAttrib(dpy_, configs[i], EGL_BUFFER_SIZE, &buffer_bpp);
        eglGetConfigAttrib(dpy_, configs[i], EGL_RED_SIZE, &red_size);
        SPDLOG_DEBUG("EGL_BUFFER_SIZE: {}", buffer_bpp);
        SPDLOG_DEBUG("EGL_RED_SIZE: {}", red_size);
        if ((buffer_bpp_ == 0 || buffer_bpp_ == buffer_bpp) && red_size < 10) {
            config_ = configs[i];
            break;
        }
    }
    free(configs);
    if (config_ == nullptr) {
        spdlog::critical("did not find config with buffer size {}", buffer_bpp_);
        exit(EXIT_FAILURE);
    }

    context_ =
            eglCreateContext(dpy_, config_, EGL_NO_CONTEXT, context_attribs_.data());

#if !defined(NDEBUG)
    egl_khr_debug_init();
#endif

    const auto extensions = eglQueryString(dpy_, EGL_EXTENSIONS);

    // setup for Damage Region Management
    if (has_egl_extension(extensions, "EGL_EXT_swap_buffers_with_damage")) {
        pfSwapBufferWithDamage_ =
                reinterpret_cast<PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC>(
                        eglGetProcAddress("eglSwapBuffersWithDamageEXT"));
    } else if (has_egl_extension(extensions,
                                 "EGL_KHR_swap_buffers_with_damage")) {
        pfSwapBufferWithDamage_ =
                reinterpret_cast<PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC>(
                        eglGetProcAddress("eglSwapBuffersWithDamageKHR"));
    }

    if (has_egl_extension(extensions, "EGL_EXT_partial_update")) {
        pfSetDamageRegion_ = reinterpret_cast<PFNEGLSETDAMAGEREGIONKHRPROC>(
                eglGetProcAddress("eglSetDamageRegionEXT"));

    } else if (has_egl_extension(extensions, "EGL_KHR_partial_update")) {
        pfSetDamageRegion_ = reinterpret_cast<PFNEGLSETDAMAGEREGIONKHRPROC>(
                eglGetProcAddress("eglSetDamageRegionKHR"));
    }

    wl_egl_window_ = wl_egl_window_create(wl_surface_, width_, height_);
    eglMakeCurrent(dpy_, egl_surface_, egl_surface_, context_);
    egl_surface_ = eglCreateWindowSurface(
            dpy_, config_, reinterpret_cast<EGLNativeWindowType>(wl_egl_window_),
            nullptr);
    eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    SPDLOG_TRACE("--Egl::Egl()");
}

/**
 * @brief Destructor for the Egl class.
 *
 * This destructor terminates the EGL display and releases resources associated
 * with the EGL thread.
 */
Egl::~Egl() {
    SPDLOG_TRACE("++Egl::~Egl()");
    eglTerminate(dpy_);
    eglReleaseThread();
    SPDLOG_TRACE("--Egl::~Egl()");
}

/**
 * \brief Make the EGL context current.
 *
 * This function checks if the current EGL context is different from the stored
 * context_. If it is different, it calls eglMakeCurrent to make the stored
 * context_ the current context.
 *
 * \return True if the context was made current successfully, false otherwise.
 */
void Egl::make_current() {
    SPDLOG_TRACE("++Egl::make_current()");
    if (eglGetCurrentContext() != context_) {
        eglMakeCurrent(dpy_, egl_surface_, egl_surface_, context_);
    }
    SPDLOG_TRACE("--Egl::make_current()");
}

/**
 * @brief Clears the current EGL context.
 *
 * This function checks if there is a current EGL context using
 * `eglGetCurrentContext()`. If a current context exists, it calls
 * `eglMakeCurrent()` with `EGL_NO_SURFACE`, `EGL_NO_SURFACE`, and
 * `EGL_NO_CONTEXT` as parameters to clear the current context.
 *
 * @return true if the current context was cleared successfully, false
 * otherwise.
 */
void Egl::clear_current() {
    SPDLOG_TRACE("++Egl::clear_current()");
    if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
        eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    SPDLOG_TRACE("--Egl::clear_current()");
}

/**
 * @brief Swaps the front and back buffers of the EGL surface.
 *
 * This function swaps the front and back buffers of the EGL surface associated
 * with the Egl object. It uses the EGL function eglSwapBuffers() internally.
 *
 * @return True if the swap was successful, false otherwise.
 */
void Egl::swap_buffers() {
    SPDLOG_TRACE("++Egl::swap_buffers()");
    eglSwapBuffers(dpy_, egl_surface_);
    SPDLOG_TRACE("--Egl::swap_buffers()");
}

void Egl::get_buffer_age(EGLint &buffer_age) {
    if (pfSwapBufferWithDamage_) {
        eglQuerySurface(dpy_, egl_surface_, EGL_BUFFER_AGE_EXT, &buffer_age);
        return;
    }
    buffer_age = 0;
}

void Egl::swap_buffers_with_damage(const EGLint *rects, EGLint n_rects) {
    if (pfSwapBufferWithDamage_) {
        pfSwapBufferWithDamage_(dpy_, egl_surface_, rects, n_rects);
    }
}

/**
 * @brief Checks if a given EGL extension is supported.
 *
 * This function searches for the specified extension name within the provided
 * extensions string. The extensions string is expected to be in a
 * space-separated format, with each extension name terminated by a space
 * character or a null terminator.
 *
 * @param extensions The extensions string to search within.
 * @param name The name of the extension to check for.
 * @return true if the extension is found, false otherwise.
 */
bool Egl::has_egl_extension(const char *extensions, const char *name) {
    const char *r = strstr(extensions, name);
    const auto len = strlen(name);
    // check that the extension name is terminated by space or null terminator
    return r != nullptr && (r[len] == ' ' || r[len] == 0);
}

/**
 * @brief Debug callback for EGL errors.
 *
 * This function is called when an EGL error occurs. It prints the error details
 * and additional information to the standard error stream (std::cerr).
 *
 * @param error The EGL error code.
 * @param command The EGL command associated with the error.
 * @param messageType The EGL message type.
 * @param threadLabel The EGL thread label.
 * @param objectLabel The EGL object label.
 * @param message The error message.
 *
 * @return None.
 */
void Egl::debug_callback(EGLenum error,
                         const char *command,
                         EGLint messageType,
                         EGLLabelKHR threadLabel,
                         EGLLabelKHR objectLabel,
                         const char *message) {
    spdlog::error("**** EGL Error");
    spdlog::error("\terror: {}", error);
    spdlog::error("\tcommand: {}", command);
    switch (error) {
        case EGL_BAD_ACCESS:
            spdlog::error("\terror: EGL_BAD_ACCESS");
            break;
        case EGL_BAD_ALLOC:
            spdlog::error("\terror: EGL_BAD_ALLOC");
            break;
        case EGL_BAD_ATTRIBUTE:
            spdlog::error("\terror: EGL_BAD_ATTRIBUTE");
            break;
        case EGL_BAD_CONFIG:
            spdlog::error("\terror: EGL_BAD_CONFIG");
            break;
        case EGL_BAD_CONTEXT:
            spdlog::error("\terror: EGL_BAD_CONTEXT");
            break;
        case EGL_BAD_CURRENT_SURFACE:
            spdlog::error("\terror: EGL_BAD_CURRENT_SURFACE");
            break;
        case EGL_BAD_DISPLAY:
            spdlog::error("\terror: EGL_BAD_DISPLAY");
            break;
        case EGL_BAD_MATCH:
            spdlog::error("\terror: EGL_BAD_MATCH");
            break;
        case EGL_BAD_NATIVE_PIXMAP:
            spdlog::error("\terror: EGL_BAD_NATIVE_PIXMAP");
            break;
        case EGL_BAD_NATIVE_WINDOW:
            spdlog::error("\terror: EGL_BAD_NATIVE_WINDOW");
            break;
        case EGL_BAD_PARAMETER:
            spdlog::error("\terror: EGL_BAD_PARAMETER");
            break;
        case EGL_BAD_SURFACE:
            spdlog::error("\terror: EGL_BAD_SURFACE");
            break;
        default:
            spdlog::error("\terror: {}", error);
            break;
    }
    spdlog::error("\tmessageType: {}", messageType);
    spdlog::error("\tthreadLabel: {}", threadLabel);
    spdlog::error("\tobjectLabel: {}", objectLabel);
    spdlog::error("\tmessage: {}", ((message == nullptr) ? "" : message));
}

/**
 * @brief Initialize the EGL debugging functionality.
 *
 * This function initializes the EGL debugging functionality by calling the
 * eglDebugMessageControlKHR function if it is available. The debug_callback
 * function is set as the callback function for EGL debug messages.
 *
 * @note This function requires that the EGL extension EGL_KHR_debug is
 * supported.
 */
void Egl::egl_khr_debug_init() {
    auto pfDebugMessageControl =
            reinterpret_cast<PFNEGLDEBUGMESSAGECONTROLKHRPROC>(
                    eglGetProcAddress("eglDebugMessageControlKHR"));

    if (pfDebugMessageControl) {
        const EGLAttrib sDebugAttribList[] = {EGL_DEBUG_MSG_CRITICAL_KHR,
                                              EGL_TRUE,
                                              EGL_DEBUG_MSG_ERROR_KHR,
                                              EGL_TRUE,
                                              EGL_DEBUG_MSG_WARN_KHR,
                                              EGL_TRUE,
                                              EGL_DEBUG_MSG_INFO_KHR,
                                              EGL_TRUE,
                                              EGL_NONE,
                                              0};

        pfDebugMessageControl(debug_callback, sDebugAttribList);
    }
}

void Egl::set_swap_interval(int interval) {
    make_current();

    EGLBoolean ret = eglSwapInterval(dpy_, interval);
    if (ret == EGL_FALSE) {
        throw std::runtime_error("eglSwapInterval failed");
    }
    clear_current();
}

void Egl::resize(int width, int height, int dx, int dy) {
    width_ = width;
    height_ = height;
    wl_egl_window_resize(wl_egl_window_, width, height, dx, dy);
}
