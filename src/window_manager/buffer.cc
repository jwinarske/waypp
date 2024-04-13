
#include "buffer.h"

Buffer::Buffer(wl_shm_pool *wl_shm_pool, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format) {
    wl_buffer_ = wl_shm_pool_create_buffer(wl_shm_pool, offset, width, height, stride, format);
    wl_buffer_add_listener(wl_buffer_, &buffer_liestener_, this);
}

Buffer::~Buffer() {
    if (wl_buffer_) {
        wl_buffer_destroy(wl_buffer_)
    }
}

void Buffer::handle_release(void *data, struct wl_buffer *wl_buffer) {

}