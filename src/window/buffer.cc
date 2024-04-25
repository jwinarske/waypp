
#include "buffer.h"

#include <wayland-client.h>
#include <sys/mman.h>

#include "anonymous_file.h"
#include "logging.h"


Buffer::Buffer(struct wl_shm *wl_shm, uint32_t format) : width_(0), height_(0), format_(format), busy_(false),
                                                         wl_shm_(wl_shm) {
}

Buffer::~Buffer() {
    if (buffer_)
        wl_buffer_destroy(buffer_);

    munmap(shm_data_, static_cast<size_t>(size_));
}

void Buffer::handle_release(void *data, struct wl_buffer * /* buffer */) {
    auto obj = static_cast<Buffer *>(data);
    obj->busy_ = false;
}

const struct wl_buffer_listener Buffer::listener_ = {
        .release = handle_release
};

int Buffer::create_shm_buffer(int width, int height) {
    auto pitch = width * 4;
    size_ = pitch * height;

    auto fd = AnonymousFile::create(size_);
    if (fd < 0) {
        spdlog::error("creating a buffer file for {} B failed: {}", size_, strerror(errno));
        return -1;
    }

    auto data = mmap(nullptr, static_cast<size_t>(size_), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        spdlog::error("mmap failed: {}", strerror(errno));
        close(fd);
        return -1;
    }

    auto pool = wl_shm_create_pool(wl_shm_, fd, size_);
    buffer_ = wl_shm_pool_create_buffer(pool, 0, width, height, pitch, format_);
    wl_buffer_add_listener(buffer_, &listener_, this);
    wl_shm_pool_destroy(pool);
    close(fd);

    shm_data_ = data;
    return 0;
}
