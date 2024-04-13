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

#include <wayland-client.h>

class XdgWindowManager;

class Buffer {
public:
    explicit Buffer(wl_shm_pool *wl_shm_pool, int32_t offset, int32_t width, int32_t height, int32_t stride,
                    uint32_t format);

    ~Buffer();

private:
    struct wl_buffer *wl_buffer_;

    /**
     * compositor releases buffer
     *
     * Sent when this wl_buffer is no longer used by the compositor.
     * The client is now free to reuse or destroy this buffer and its
     * backing storage.
     *
     * If a client receives a release event before the frame callback
     * requested in the same wl_surface.commit that attaches this
     * wl_buffer to a surface, then the client is immediately free to
     * reuse the buffer and its backing storage, and does not need a
     * second buffer for the next surface content update. Typically
     * this is possible, when the compositor maintains a copy of the
     * wl_surface contents, e.g. as a GL texture. This is an important
     * optimization for GL(ES) compositors with wl_shm clients.
     */
    static void handle_release(void *data, struct wl_buffer *wl_buffer);

    static constexpr wl_buffer_listener buffer_liestener_ = {
            .release = handle_release,
    };
};
