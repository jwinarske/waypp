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

#ifndef SRC_DISPLAY_H_
#define SRC_DISPLAY_H_

#include <functional>
#include <memory>
#include <optional>
#include <map>
#include <cstdint>

#include <wayland-client.h>
#include <glib-2.0/glib.h>

#include "output.h"
#include "seat/seat.h"

class Output;

class Seat;

class Display {
public:
    explicit Display(GMainContext *context = nullptr, bool enable_cursor = true, const char *name = nullptr);

    ~Display();

    [[nodiscard]] struct wl_display *get_display() const { return wl_display_; }

    [[nodiscard]] const std::map<struct wl_seat *, std::unique_ptr<Seat>> &get_seats() const { return wl_seats_; }

    [[nodiscard]] const std::map<struct wl_output *, std::unique_ptr<Output>> &
    get_outputs() const { return wl_outputs_; }

    struct wl_compositor *get_compositor() { return wl_compositor_; }

    void add_registrar_callback(const std::function<void(void *data, struct wl_registry *registry,
                                                         uint32_t name,
                                                         const char *interface,
                                                         uint32_t version)> &callback, void *data);

    friend class Cursor;

    friend class Window;

    friend class WindowEgl;

    friend class WindowVulkan;

    friend class WindowManager;

private:
    struct wl_compositor *wl_compositor_{};
    uint32_t compositor_version_{};
    struct wl_subcompositor *wl_subcompositor_{};
    uint32_t subcompositor_version_{};
    struct wl_display *wl_display_{};
    struct wl_registry *wl_registry_{};
    struct wl_shm *wl_shm_{};

    GMainContext *context_;
    bool enable_cursor_;

    std::map<struct wl_output *, std::unique_ptr<Output>> wl_outputs_;
    std::map<struct wl_seat *, std::unique_ptr<Seat>> wl_seats_;

    bool has_xrgb_{};
    std::optional<bool> buffer_scaling_enabled_;

    std::vector<std::pair<std::function<void(void * /*data */, struct wl_registry * /* registry */,
                                             uint32_t /* name */,
                                             const char * /* interface */,
                                             uint32_t /* version */)>, void * /* data */>> callbacks_;

    struct wl_compositor *get_compositor() const { return wl_compositor_; }

    static void registry_handle_global(void *data,
                                       struct wl_registry *registry,
                                       uint32_t name,
                                       const char *interface,
                                       uint32_t version);

    static void registry_handle_global_remove(void *data,
                                              struct wl_registry *reg,
                                              uint32_t id);

    static const struct wl_registry_listener listener_;

    static void shm_format(void * /* data */,
                           struct wl_shm * /* wl_shm */,
                           uint32_t /* format */);

    static const struct wl_shm_listener shm_listener_;

};


#endif //SRC_DISPLAY_H_