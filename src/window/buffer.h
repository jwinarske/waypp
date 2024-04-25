
#pragma once

#include <cstdint>
#include <sys/types.h>


class Buffer {
public:
    Buffer(struct wl_shm *wl_shm, uint32_t format);

    ~Buffer();

    [[nodiscard]] int get_width() const { return width_; }

    [[nodiscard]] int get_height() const { return height_; }

    [[nodiscard]] uint32_t get_format() const { return format_; }

    int create_shm_buffer(int width, int height);

    [[nodiscard]] bool is_busy() const { return busy_; }

    void *get_shm_data() { return shm_data_; }

    [[nodiscard]] struct wl_buffer *get_wl_buffer() const { return buffer_; }

    void set_busy() { busy_ = true; }

private:
    int width_;
    int height_;
    uint32_t format_;
    bool busy_;

    int size_;
    struct wl_shm *wl_shm_;
    struct wl_buffer *buffer_{};

    void *shm_data_{};

    static void handle_release(void *data, struct wl_buffer *buffer);

    static const struct wl_buffer_listener listener_;
};