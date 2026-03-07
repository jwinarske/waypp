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
// clang-tidy: The checks below are suppressed for the entire example file.
// These examples interface directly with Vulkan, EGL, Wayland, and OpenGL
// C APIs that fundamentally require pointer arithmetic, non-const globals
// for signal-handler flags, array-to-pointer decay, and reinterpret_cast
// at C API boundaries. Suppressing per-line would add hundreds of NOLINT
// annotations with no increase in safety — the patterns are intentional
// and audited.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,
//             cppcoreguidelines-pro-bounds-pointer-arithmetic,
//             cppcoreguidelines-pro-bounds-array-to-pointer-decay,
//             cppcoreguidelines-pro-bounds-constant-array-index,
//             cppcoreguidelines-pro-type-reinterpret-cast)

// Danil, 2021+ Vulkan shader launcher, self
// https://github.com/danilw/vulkan-shadertoy-launcher The MIT License

#include "vk_error_print.h"

void vk_error_data_set_vkresult(struct vk_error_data* error,
                                VkResult vkresult,
                                const char* file,
                                unsigned int line) {
  if (vkresult == 0)
    return;

  if (error->type != VK_ERROR_SUCCESS &&
      !(error->type == VK_ERROR_VKRESULT_WARNING && vkresult < 0))
    return;

  *error = (struct vk_error_data){
      .type = vkresult < 0 ? VK_ERROR_VKRESULT : VK_ERROR_VKRESULT_WARNING,
      .vkresult = vkresult,
      .file = file,
      .line = line,
  };
}

void vk_error_data_set_errno(struct vk_error_data* error,
                             int err_no,
                             const char* file,
                             unsigned int line) {
  if (err_no == 0)
    return;

  if (error->type != VK_ERROR_SUCCESS &&
      error->type != VK_ERROR_VKRESULT_WARNING)
    return;

  *error = (struct vk_error_data){
      .type = VK_ERROR_ERRNO,
      .err_no = err_no,
      .file = file,
      .line = line,
  };
}

bool vk_error_data_merge(struct vk_error_data* error,
                         struct vk_error_data* other) {
  if (other->type == VK_ERROR_SUCCESS)
    return false;

  if (error->type != VK_ERROR_SUCCESS &&
      !(error->type == VK_ERROR_VKRESULT_WARNING &&
        (other->type == VK_ERROR_VKRESULT || other->type == VK_ERROR_ERRNO)))
    return false;

  *error = *other;
  return true;
}

bool vk_error_is_success(struct vk_error* error) {
  return error->error.type == VK_ERROR_SUCCESS;
}

bool vk_error_is_warning(struct vk_error* error) {
  return error->error.type == VK_ERROR_VKRESULT_WARNING;
}

bool vk_error_is_error(struct vk_error* error) {
  return !vk_error_is_success(error) && !vk_error_is_warning(error);
}

static const char* VkResult_string(VkResult res) {
  switch (res) {
    case VK_SUCCESS:
      return "Success";
    case VK_NOT_READY:
      return "Not ready";
    case VK_TIMEOUT:
      return "Timeout";
    case VK_EVENT_SET:
      return "Event set";
    case VK_EVENT_RESET:
      return "Event reset";
    case VK_INCOMPLETE:
      return "Incomplete";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "Out of host memory";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "Out of device memory";
    case VK_ERROR_INITIALIZATION_FAILED:
      return "Initialization failed";
    case VK_ERROR_DEVICE_LOST:
      return "Device lost";
    case VK_ERROR_MEMORY_MAP_FAILED:
      return "Memory map failed";
    case VK_ERROR_LAYER_NOT_PRESENT:
      return "Layer not present";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
      return "Extension not present";
    case VK_ERROR_FEATURE_NOT_PRESENT:
      return "Feature not present";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
      return "Incompatible driver";
    case VK_ERROR_TOO_MANY_OBJECTS:
      return "Too many objects";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
      return "Format not supported";
    case VK_ERROR_SURFACE_LOST_KHR:
      return "Surface lost";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
      return "Native window is in use";
    case VK_SUBOPTIMAL_KHR:
      return "Suboptimal";
    case VK_ERROR_OUT_OF_DATE_KHR:
      return "Surface is out of date";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
      return "Incompatible display";
    case VK_ERROR_VALIDATION_FAILED_EXT:
      return "Validation failed";
    default:
      return "Unrecognized error";
  }
}

#if defined(VK_USE_PLATFORM_WIN32_KHR)
void win_error(char* iout, char* iout2) {
  int msgboxID = MessageBox(NULL, (LPCSTR)iout, (LPCSTR)iout2,
                            MB_ICONHAND | MB_DEFBUTTON1);
}

#endif

void print_error(FILE* fout,
                 struct vk_error_data* error_data,
                 const char* prefix) {
  fprintf(fout, "%s:%u: %s", error_data->file, error_data->line, prefix);
  switch (error_data->type) {
    case VK_ERROR_VKRESULT_WARNING:
    case VK_ERROR_VKRESULT:
      fprintf(fout, "%s (VkResult %d)\n", VkResult_string(error_data->vkresult),
              error_data->vkresult);
      break;
    case VK_ERROR_ERRNO:
      fprintf(fout, "%s (errno %d)\n", std::strerror(error_data->err_no),
              error_data->err_no);
      break;
    default:
      fprintf(fout, "<internal error>\n");
      break;
  }
}

void vk_error_fprintf(FILE* fout,
                      struct vk_error* error,
                      const char* fmt,
                      ...) {
  if (error->error.type == VK_ERROR_SUCCESS)
    return;

  va_list args;
  va_start(args, fmt);
  vfprintf(fout, fmt, args);
  va_end(args);

  print_error(fout, &error->error, "");
  if (error->sub_error.type != VK_ERROR_SUCCESS) {
    print_error(fout, &error->sub_error, "    Resulting from this error: ");
  }
}

/* The following functions get a readable string out of the Vulkan standard
 * enums */
const char* vk_VkPhysicalDeviceType_string(VkPhysicalDeviceType type) {
  switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
      return "Neither GPU nor CPU";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return "CPU";
    default:
      return "Unrecognized device type";
  }
}
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,
//           cppcoreguidelines-pro-bounds-pointer-arithmetic,
//           cppcoreguidelines-pro-bounds-array-to-pointer-decay,
//           cppcoreguidelines-pro-bounds-constant-array-index,
//           cppcoreguidelines-pro-type-reinterpret-cast)
