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

#include "waypp/window/buffer.h"

#include <cerrno>
#include <cstring>

#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "anonymous_file.h"

#include "logging/logging.h"

Buffer::Buffer(wl_shm* wl_shm)
    : width_(0), height_(0), busy_(false), wl_shm_(wl_shm) {}

Buffer::~Buffer() {
  if (shm_data_ != nullptr && shm_data_ != MAP_FAILED) {
    munmap(shm_data_, static_cast<size_t>(size_));
  }

  if (buffer_) {
    DLOG_TRACE("[Buffer] wl_buffer_destroy(buffer_)");
    wl_buffer_destroy(buffer_);
  }
}

void Buffer::destroy() {
  if (shm_data_ != nullptr && shm_data_ != MAP_FAILED) {
    munmap(shm_data_, static_cast<size_t>(size_));
    shm_data_ = nullptr;
  }
  if (buffer_) {
    DLOG_TRACE("[Buffer] wl_buffer_destroy(buffer_) [resize]");
    wl_buffer_destroy(buffer_);
    buffer_ = nullptr;
  }
  width_ = 0;
  height_ = 0;
  size_ = 0;
  busy_ = false;
}

void Buffer::handle_release(void* data, wl_buffer* /* buffer */) {
  const auto obj = static_cast<Buffer*>(data);
  obj->busy_ = false;
}

const wl_buffer_listener Buffer::listener_ = {.release = handle_release};

int Buffer::create_shm_buffer(int width, int height, uint32_t format) {
  if (buffer_) {
    LOG_ERROR("shm_buffer already exists");
    return -1;
  }

  width_ = width;
  height_ = height;
  format_ = format;

  // Compute bytes-per-pixel from the wl_shm_format.
  // Only formats explicitly handled here are accepted; any unknown format
  // returns an error rather than silently using a wrong stride.
  int bpp;
  switch (static_cast<wl_shm_format>(format)) {
    // 32-bit formats
    case WL_SHM_FORMAT_ARGB8888:
    case WL_SHM_FORMAT_XRGB8888:
    case WL_SHM_FORMAT_ABGR8888:
    case WL_SHM_FORMAT_XBGR8888:
    case WL_SHM_FORMAT_RGBA8888:
    case WL_SHM_FORMAT_RGBX8888:
    case WL_SHM_FORMAT_BGRA8888:
    case WL_SHM_FORMAT_BGRX8888:
    case WL_SHM_FORMAT_ARGB2101010:
    case WL_SHM_FORMAT_XRGB2101010:
    case WL_SHM_FORMAT_ABGR2101010:
    case WL_SHM_FORMAT_XBGR2101010:
    case WL_SHM_FORMAT_RGBA1010102:
    case WL_SHM_FORMAT_RGBX1010102:
    case WL_SHM_FORMAT_BGRA1010102:
    case WL_SHM_FORMAT_BGRX1010102:
      bpp = 4;
      break;
    // 24-bit formats
    case WL_SHM_FORMAT_RGB888:
    case WL_SHM_FORMAT_BGR888:
      bpp = 3;
      break;
    // 16-bit formats
    case WL_SHM_FORMAT_RGB565:
    case WL_SHM_FORMAT_BGR565:
    case WL_SHM_FORMAT_ARGB1555:
    case WL_SHM_FORMAT_XRGB1555:
    case WL_SHM_FORMAT_RGBA5551:
    case WL_SHM_FORMAT_RGBX5551:
    case WL_SHM_FORMAT_BGRA5551:
    case WL_SHM_FORMAT_BGRX5551:
    case WL_SHM_FORMAT_ARGB4444:
    case WL_SHM_FORMAT_XRGB4444:
    case WL_SHM_FORMAT_RGBA4444:
    case WL_SHM_FORMAT_RGBX4444:
    case WL_SHM_FORMAT_BGRA4444:
    case WL_SHM_FORMAT_BGRX4444:
    case WL_SHM_FORMAT_YUYV:
    case WL_SHM_FORMAT_YVYU:
    case WL_SHM_FORMAT_UYVY:
    case WL_SHM_FORMAT_VYUY:
    case WL_SHM_FORMAT_AYUV:
      bpp = 2;
      break;
    // 8-bit formats
    case WL_SHM_FORMAT_C8:
    case WL_SHM_FORMAT_RGB332:
    case WL_SHM_FORMAT_BGR233:
      bpp = 1;
      break;
    default:
      LOG_ERROR("[Buffer] unsupported wl_shm_format 0x{:08X} — "
                "cannot compute stride; buffer not created", format);
      return -1;
  }

  const int pitch = width * bpp;
  size_ = pitch * height;

  const auto fd = AnonymousFile::create(size_);
  if (fd < 0) {
    LOG_ERROR("creating a buffer file for {} B failed: {}", size_,
              std::strerror(errno));
    return -1;
  }

  const auto data = mmap(nullptr, static_cast<size_t>(size_),
                         PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    LOG_ERROR("mmap failed: {}", std::strerror(errno));
    close(fd);
    return -1;
  }

  const auto wl_shm_pool = wl_shm_create_pool(wl_shm_, fd, size_);
  if (!wl_shm_pool) {
    LOG_ERROR("failed to create Wayland SHM pool");
    close(fd);
    return -1;
  }
  buffer_ =
      wl_shm_pool_create_buffer(wl_shm_pool, 0, width, height, pitch, format_);
  if (!buffer_) {
    LOG_ERROR("failed to create Wayland SHM buffer");
    wl_shm_pool_destroy(wl_shm_pool);
    close(fd);
    return -1;
  }
  DLOG_TRACE("[Buffer] wl_shm_pool_destroy(wl_shm_pool)");
  wl_shm_pool_destroy(wl_shm_pool);
  close(fd);

  wl_buffer_add_listener(buffer_, &listener_, this);

  shm_data_ = data;
  return 0;
}
