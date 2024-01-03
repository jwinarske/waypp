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

#include <iostream>

#include <cstdlib>
#include <cstring>


/**
 * @brief The Egl class represents an EGL object used for OpenGL rendering.
 *
 * This class provides functionality for initializing EGL, choosing an EGL configuration,
 * creating an EGL context, and managing various EGL extensions.
 */
Egl::Egl(struct wl_display *display) {
    dpy_ = eglGetDisplay(display);
    EGLBoolean ret = eglInitialize(dpy_, &major_, &minor_);
    if (ret == EGL_FALSE) {
        throw std::runtime_error("eglInitialize failed.");
    }

    ret = eglBindAPI(EGL_OPENGL_ES_API);
    if (ret == EGL_FALSE) {
        throw std::runtime_error("eglBindAPI failed.");
    }

    EGLint count;
    ret = eglGetConfigs(dpy_, nullptr, 0, &count);
    if (ret == EGL_FALSE) {
        throw std::runtime_error("eglGetConfigs failed.");
    }

    auto *configs = static_cast<EGLConfig *>(
            calloc(static_cast<size_t>(count), sizeof(EGLConfig)));

    EGLint n;
    ret = eglChooseConfig(dpy_, kEglConfigAttribs.data(), configs, count, &n);
    if (ret == EGL_FALSE) {
        throw std::runtime_error("eglChooseConfig failed");
    }

    EGLint size;
    for (EGLint i = 0; i < n; i++) {
        ret = eglGetConfigAttrib(dpy_, configs[i], EGL_BUFFER_SIZE, &size);
        if (ret == EGL_FALSE) {
            throw std::runtime_error("eglGetConfigAttrib failed");
        }
        if (buffer_size_ <= size) {
            std::memcpy(&config_, &configs[i], sizeof(EGLConfig));
            break;
        }
    }
    free(configs);

    context_ = eglCreateContext(dpy_, config_, EGL_NO_CONTEXT,
                                kEglContextAttribs.data());

    resource_context_ =
            eglCreateContext(dpy_, config_, context_, kEglContextAttribs.data());

    texture_context_ =
            eglCreateContext(dpy_, config_, context_, kEglContextAttribs.data());

    // frame callback requires non-blocking
#if 0 // Failing
    ret = eglSwapInterval(dpy_, 0);
    if (ret == EGL_FALSE) {
        throw std::runtime_error("eglSwapInterval failed");
    }
#endif

    (void) make_current();

#if !defined(NDEBUG)
    egl_khr_debug_init();
#endif

    const auto extensions = eglQueryString(dpy_, EGL_EXTENSIONS);

    // setup for Damage Region Management
    if (has_egl_extension(extensions, "EGL_EXT_swap_buffers_with_damage")) {
        pfSwapBufferWithDamage_ =
                reinterpret_cast<PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC>(
                        eglGetProcAddress("eglSwapBuffersWithDamageEXT"));
    } else if (has_egl_extension(extensions, "EGL_KHR_swap_buffers_with_damage")) {
        pfSwapBufferWithDamage_ =
                reinterpret_cast<PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC>(
                        eglGetProcAddress("eglSwapBuffersWithDamageKHR"));
    }

    if (has_egl_extension(extensions, "EGL_KHR_partial_update")) {
        pfSetDamageRegion_ = reinterpret_cast<PFNEGLSETDAMAGEREGIONKHRPROC>(
                eglGetProcAddress("eglSetDamageRegionKHR"));
    }

    has_egl_ext_buffer_age_ = has_egl_extension(extensions, "EGL_EXT_buffer_age");

    (void) clear_current();
}

/**
 * @brief Destructor for the Egl class.
 *
 * This destructor terminates the EGL display and releases resources associated with the EGL thread.
 */
Egl::~Egl() {
    eglTerminate(dpy_);
    eglReleaseThread();
}

/**
 * \brief Make the EGL context current.
 *
 * This function checks if the current EGL context is different from the stored context_.
 * If it is different, it calls eglMakeCurrent to make the stored context_ the current context.
 *
 * \return True if the context was made current successfully, false otherwise.
 */
bool Egl::make_current() const {
    if (eglGetCurrentContext() != context_) {
        eglMakeCurrent(dpy_, egl_surface_, egl_surface_, context_);
    }
    return true;
}

/**
 * @brief Clears the current EGL context.
 *
 * This function checks if there is a current EGL context using `eglGetCurrentContext()`.
 * If a current context exists, it calls `eglMakeCurrent()` with
 * `EGL_NO_SURFACE`, `EGL_NO_SURFACE`, and `EGL_NO_CONTEXT` as parameters to clear the current context.
 *
 * @return true if the current context was cleared successfully, false otherwise.
 */
bool Egl::clear_current() const {
    if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
        eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    return true;
}

/**
 * @brief Swaps the front and back buffers of the EGL surface.
 *
 * This function swaps the front and back buffers of the EGL surface associated with the Egl object. It uses the EGL function eglSwapBuffers() internally.
 *
 * @return True if the swap was successful, false otherwise.
 */
bool Egl::swap_buffers() const {
    eglSwapBuffers(dpy_, egl_surface_);
    return true;
}

/**
 * @brief Checks if the resource context is currently active and makes it current if not.
 *
 * This function checks if the resource context is currently active using eglGetCurrentContext().
 * If the current context is not equal to the resource context, it calls eglMakeCurrent() to make
 * the resource context current. Finally, it returns true to indicate that the operation was successful.
 *
 * @return true if the resource context is successfully made current, false otherwise.
 */
bool Egl::make_resource_current() const {
    if (eglGetCurrentContext() != context_) {
        eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, resource_context_);
    }
    return true;
}

/**
 * @brief Checks if the texture context is the current context and makes it the current context if not.
 *
 * This function checks if the texture context is the current context using eglGetCurrentContext() function. If it is not,
 * it calls eglMakeCurrent() function to set the texture context as the current context.
 *
 * @return true if the texture context is made the current context or if it is already the current context, false otherwise.
 */
bool Egl::make_texture_current() const {
    if (eglGetCurrentContext() != texture_context_) {
        eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, texture_context_);
    }
    return true;
}

/**
 * @brief Checks if a given EGL extension is supported.
 *
 * This function searches for the specified extension name within the provided extensions string.
 * The extensions string is expected to be in a space-separated format, with each extension name
 * terminated by a space character or a null terminator.
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
 * This function is called when an EGL error occurs. It prints the error details and additional
 * information to the standard error stream (std::cerr).
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
    std::cerr << "**** EGL Error" << std::endl;
    std::cerr << "\terror: " << error << std::endl;
    std::cerr << "\tcommand: " << command << std::endl;
    switch (error) {
        case EGL_BAD_ACCESS:
            std::cerr << "\terror: EGL_BAD_ACCESS" << std::endl;
            break;
        case EGL_BAD_ALLOC:
            std::cerr << "\terror: EGL_BAD_ALLOC" << std::endl;
            break;
        case EGL_BAD_ATTRIBUTE:
            std::cerr << "\terror: EGL_BAD_ATTRIBUTE" << std::endl;
            break;
        case EGL_BAD_CONFIG:
            std::cerr << "\terror: EGL_BAD_CONFIG" << std::endl;
            break;
        case EGL_BAD_CONTEXT:
            std::cerr << "\terror: EGL_BAD_CONTEXT" << std::endl;
            break;
        case EGL_BAD_CURRENT_SURFACE:
            std::cerr << "\terror: EGL_BAD_CURRENT_SURFACE" << std::endl;
            break;
        case EGL_BAD_DISPLAY:
            std::cerr << "\terror: EGL_BAD_DISPLAY" << std::endl;
            break;
        case EGL_BAD_MATCH:
            std::cerr << "\terror: EGL_BAD_MATCH" << std::endl;
            break;
        case EGL_BAD_NATIVE_PIXMAP:
            std::cerr << "\terror: EGL_BAD_NATIVE_PIXMAP" << std::endl;
            break;
        case EGL_BAD_NATIVE_WINDOW:
            std::cerr << "\terror: EGL_BAD_NATIVE_WINDOW" << std::endl;
            break;
        case EGL_BAD_PARAMETER:
            std::cerr << "\terror: EGL_BAD_PARAMETER" << std::endl;
            break;
        case EGL_BAD_SURFACE:
            std::cerr << "\terror: EGL_BAD_SURFACE" << std::endl;
            break;
        default:
            std::cerr << "\terror: " << error << std::endl;
            break;
    }
    std::cerr << "\tmessageType: " << messageType << std::endl;
    std::cerr << "\tthreadLabel: " << threadLabel << std::endl;
    std::cerr << "\tobjectLabel: " << objectLabel << std::endl;
    std::cerr << "\tmessage: " << ((message == nullptr) ? "" : message) << std::endl;
}

/**
 * @brief Initialize the EGL debugging functionality.
 *
 * This function initializes the EGL debugging functionality by calling the eglDebugMessageControlKHR function
 * if it is available. The debug_callback function is set as the callback function for EGL debug messages.
 *
 * @note This function requires that the EGL extension EGL_KHR_debug is supported.
 */
void Egl::egl_khr_debug_init() {
    auto pfDebugMessageControl =
            reinterpret_cast<PFNEGLDEBUGMESSAGECONTROLKHRPROC>(
                    eglGetProcAddress("eglDebugMessageControlKHR"));

    if (pfDebugMessageControl) {

        const EGLAttrib sDebugAttribList[] = {
                EGL_DEBUG_MSG_CRITICAL_KHR,
                EGL_TRUE,
                EGL_DEBUG_MSG_ERROR_KHR,
                EGL_TRUE,
                EGL_DEBUG_MSG_WARN_KHR,
                EGL_TRUE,
                EGL_DEBUG_MSG_INFO_KHR,
                EGL_TRUE,
                EGL_NONE,
                0
        };

        pfDebugMessageControl(debug_callback, sDebugAttribList);
    }
}
