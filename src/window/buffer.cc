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
#include <wayland-client.h>
#include <unistd.h>

#include "anonymous_file.h"

#include "logging/logging.h"

Buffer::Buffer(struct wl_shm *wl_shm)
        : width_(0), height_(0), busy_(false), wl_shm_(wl_shm) {}

Buffer::~Buffer() {
    munmap(shm_data_, static_cast<size_t>(size_));

    if (buffer_) {
        DLOG_TRACE("[Buffer] wl_buffer_destroy(buffer_)");
        wl_buffer_destroy(buffer_);
    }
}

void Buffer::handle_release(void *data, struct wl_buffer * /* buffer */) {
    auto obj = static_cast<Buffer *>(data);
    obj->busy_ = false;
}

const struct wl_buffer_listener Buffer::listener_ = {.release = handle_release};

int Buffer::create_shm_buffer(int width, int height, uint32_t format) {
    if (buffer_) {
        LOG_ERROR("shm_buffer already exists");
        return -1;
    }

    width_ = width;
    height_ = height;
    format_ = format;

    auto pitch = width * 4;
    size_ = pitch * height;

    auto fd = AnonymousFile::create(size_);
    if (fd < 0) {
        LOG_ERROR("creating a buffer file for {} B failed: {}", size_,
                  std::strerror(errno));
        return -1;
    }

    auto data = mmap(nullptr, static_cast<size_t>(size_), PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        LOG_ERROR("mmap failed: {}", std::strerror(errno));
        close(fd);
        return -1;
    }

    auto wl_shm_pool = wl_shm_create_pool(wl_shm_, fd, size_);
    buffer_ =
            wl_shm_pool_create_buffer(wl_shm_pool, 0, width, height, pitch, format_);
    DLOG_TRACE("[Buffer] wl_shm_pool_destroy(wl_shm_pool)");
    wl_shm_pool_destroy(wl_shm_pool);
    close(fd);

    wl_buffer_add_listener(buffer_, &listener_, this);

    shm_data_ = data;
    return 0;
}
