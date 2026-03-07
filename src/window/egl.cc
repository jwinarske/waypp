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

#include "waypp/window/egl.h"

#include <wayland-egl.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "logging/logging.h"
using waypp::Egl;

/**
 * @brief The Egl class represents an EGL object used for OpenGL rendering.
 *
 * This class provides functionality for initializing EGL, choosing an EGL
 * configuration, creating an EGL context, and managing various EGL extensions.
 */
// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
// EGL and Wayland EGL C APIs require reinterpret_cast for native handle types
// and pointer arithmetic for the idiomatic vector(ptr, ptr+n) constructor.
Egl::Egl(wl_display* display,
         wl_surface* wl_surface,
         const int width,
         const int height,
         config* config)
    : dpy_(eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(display))),
      context_attribs_(config->context_attribs,
                       config->context_attribs + config->context_attribs_size),
      config_attribs_(config->config_attribs,
                      config->config_attribs + config->config_attribs_size),
      buffer_bpp_(config->buffer_bpp),
      wl_surface_(wl_surface),
      width_(width),
      height_(height) {
  // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
  DLOG_TRACE("++Egl::Egl()");
  EGLBoolean ret = eglInitialize(dpy_, &major_, &minor_);
  if (ret == EGL_FALSE) {
    throw std::runtime_error("eglInitialize failed.");
  }

  ret = eglBindAPI(config->type);
  if (ret != EGL_TRUE) {
    throw std::runtime_error("eglBindAPI failed.");
  }

  EGLint count = 0;
  eglGetConfigs(dpy_, nullptr, 0, &count);
  DLOG_DEBUG("EGL has {} configs", count);

  // Guard against a compositor returning zero, negative, or a pathologically
  // large count (EGLint is signed 32-bit, so negative values are possible).
  if (count <= 0 || count > 1024) {
    throw std::runtime_error("EGL Config: unexpected config count " +
                             std::to_string(count));
  }

  // std::vector provides automatic cleanup under any exit path (return, throw,
  // or future code additions), eliminating the manual free() calls and the
  // exception-safety leak that a calloc/free pair would have.
  std::vector<EGLConfig> configs(static_cast<std::size_t>(count));

  EGLint n = 0;
  ret =
      eglChooseConfig(dpy_, config_attribs_.data(), configs.data(), count, &n);
  if (n == 0) {
    throw std::runtime_error("EGL Config: Check Config Attributes");
  }

  EGLint red_size;
  for (EGLint i = 0; i < n; i++) {
    eglGetConfigAttrib(dpy_, configs[static_cast<std::size_t>(i)],
                       EGL_BUFFER_SIZE, &config->buffer_bpp);
    eglGetConfigAttrib(dpy_, configs[static_cast<std::size_t>(i)], EGL_RED_SIZE,
                       &red_size);
    DLOG_DEBUG("EGL_BUFFER_SIZE: {}", config->buffer_bpp);
    DLOG_DEBUG("EGL_RED_SIZE: {}", red_size);
    if ((buffer_bpp_ == 0 || buffer_bpp_ == config->buffer_bpp) &&
        red_size < 10) {
      config_ = configs[static_cast<std::size_t>(i)];
      break;
    }
  }
  if (config_ == nullptr) {
    throw std::runtime_error("did not find config with buffer size " +
                             std::to_string(buffer_bpp_));
  }

  context_ =
      eglCreateContext(dpy_, config_, EGL_NO_CONTEXT, context_attribs_.data());

#if !defined(NDEBUG)
  egl_khr_debug_init();
#endif

  const auto extensions = eglQueryString(dpy_, EGL_EXTENSIONS);

  // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) --
  // eglGetProcAddress returns void*(__eglMustCastToProperFunctionPointerType);
  // EGL spec mandates reinterpret_cast to the typed function pointer.
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
  if (!wl_egl_window_) {
    throw std::runtime_error("failed to create Wayland EGL window");
  }
  egl_surface_ = eglCreateWindowSurface(
      dpy_, config_, reinterpret_cast<EGLNativeWindowType>(wl_egl_window_),
      nullptr);
  // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
  if (egl_surface_ == EGL_NO_SURFACE) {
    throw std::runtime_error("failed to create EGL window surface");
  }
  eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  DLOG_TRACE("--Egl::Egl()");
}

/**
 * @brief Destructor for the Egl class.
 *
 * This destructor terminates the EGL display and releases resources associated
 * with the EGL thread.
 */
Egl::~Egl() {
  DLOG_TRACE("++Egl::~Egl()");
  eglTerminate(dpy_);
  eglReleaseThread();
  DLOG_TRACE("--Egl::~Egl()");
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
  DLOG_TRACE("++Egl::make_current()");
  if (eglGetCurrentContext() != context_) {
    eglMakeCurrent(dpy_, egl_surface_, egl_surface_, context_);
  }
  DLOG_TRACE("--Egl::make_current()");
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
  DLOG_TRACE("++Egl::clear_current()");
  if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
    eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  }
  DLOG_TRACE("--Egl::clear_current()");
}

/**
 * @brief Swaps the front and back buffers of the EGL surface.
 *
 * This function swaps the front and back buffers of the EGL surface associated
 * with the Egl object. It uses the EGL function eglSwapBuffers() internally.
 *
 * @return True if the swap was successful, false otherwise.
 */
void Egl::swap_buffers() const {
  DLOG_TRACE("++Egl::swap_buffers()");
  eglSwapBuffers(dpy_, egl_surface_);
  DLOG_TRACE("--Egl::swap_buffers()");
}

void Egl::get_buffer_age(EGLint& buffer_age) const {
  if (pfSwapBufferWithDamage_) {
    eglQuerySurface(dpy_, egl_surface_, EGL_BUFFER_AGE_EXT, &buffer_age);
    return;
  }
  buffer_age = 0;
}

void Egl::swap_buffers_with_damage(EGLint* rects, const EGLint n_rects) const {
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
bool Egl::has_egl_extension(const char* extensions, const char* name) {
  if (!extensions || !name)
    return false;
  // Search for the name as a whole token — terminated by space or NUL.
  // Use string_view to avoid pointer arithmetic on the raw char* result.
  const std::string_view exts(extensions);
  const std::string_view needle(name);
  std::string_view::size_type pos = 0;
  while ((pos = exts.find(needle, pos)) != std::string_view::npos) {
    const auto end = pos + needle.size();
    if (end == exts.size() || exts[end] == ' ')
      return true;
    pos = end;
  }
  return false;
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
                         const char* command,
                         EGLint messageType,
                         EGLLabelKHR threadLabel,
                         EGLLabelKHR objectLabel,
                         const char* message) {
  LOG_ERROR("**** EGL Error");
  LOG_ERROR("\terror: {}", error);
  LOG_ERROR("\tcommand: {}", command);
  switch (error) {
    case EGL_BAD_ACCESS:
      LOG_ERROR("\terror: EGL_BAD_ACCESS");
      break;
    case EGL_BAD_ALLOC:
      LOG_ERROR("\terror: EGL_BAD_ALLOC");
      break;
    case EGL_BAD_ATTRIBUTE:
      LOG_ERROR("\terror: EGL_BAD_ATTRIBUTE");
      break;
    case EGL_BAD_CONFIG:
      LOG_ERROR("\terror: EGL_BAD_CONFIG");
      break;
    case EGL_BAD_CONTEXT:
      LOG_ERROR("\terror: EGL_BAD_CONTEXT");
      break;
    case EGL_BAD_CURRENT_SURFACE:
      LOG_ERROR("\terror: EGL_BAD_CURRENT_SURFACE");
      break;
    case EGL_BAD_DISPLAY:
      LOG_ERROR("\terror: EGL_BAD_DISPLAY");
      break;
    case EGL_BAD_MATCH:
      LOG_ERROR("\terror: EGL_BAD_MATCH");
      break;
    case EGL_BAD_NATIVE_PIXMAP:
      LOG_ERROR("\terror: EGL_BAD_NATIVE_PIXMAP");
      break;
    case EGL_BAD_NATIVE_WINDOW:
      LOG_ERROR("\terror: EGL_BAD_NATIVE_WINDOW");
      break;
    case EGL_BAD_PARAMETER:
      LOG_ERROR("\terror: EGL_BAD_PARAMETER");
      break;
    case EGL_BAD_SURFACE:
      LOG_ERROR("\terror: EGL_BAD_SURFACE");
      break;
    default:
      LOG_ERROR("\terror: {}", error);
      break;
  }
  LOG_ERROR("\tmessageType: {}", messageType);
  LOG_ERROR("\tthreadLabel: {}", threadLabel);
  LOG_ERROR("\tobjectLabel: {}", objectLabel);
  LOG_ERROR("\tmessage: {}", ((message == nullptr) ? "" : message));
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
  // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  // eglGetProcAddress requires reinterpret_cast; pfDebugMessageControl takes a
  // raw C array (EGLAttrib[]) which decays to a pointer per the EGL C API.
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
  // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
}

void Egl::set_swap_interval(const int interval) {
  make_current();

  if (const EGLBoolean ret = eglSwapInterval(dpy_, interval);
      ret == EGL_FALSE) {
    throw std::runtime_error("eglSwapInterval failed");
  }
  clear_current();
}

void Egl::resize(const int width,
                 const int height,
                 const int dx,
                 const int dy) {
  width_ = width;
  height_ = height;
  wl_egl_window_resize(wl_egl_window_, width, height, dx, dy);
}
