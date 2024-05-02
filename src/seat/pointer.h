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

#include <list>
#include <optional>

#include <wayland-client.h>

class Pointer;

class PointerObserver {
public:
    virtual ~PointerObserver() = default;

    virtual void notify_pointer_enter(Pointer *pointer,
                                      struct wl_pointer *wl_pointer,
                                      uint32_t serial,
                                      struct wl_surface *surface,
                                      double sx,
                                      double sy) = 0;

    virtual void notify_pointer_leave(Pointer *pointer,
                                      struct wl_pointer *wl_pointer,
                                      uint32_t serial,
                                      struct wl_surface *surface) = 0;

    virtual void notify_pointer_motion(Pointer *pointer,
                                       struct wl_pointer *wl_pointer,
                                       uint32_t time,
                                       double sx,
                                       double sy) = 0;

    virtual void notify_pointer_button(Pointer *pointer,
                                       struct wl_pointer *wl_pointer,
                                       uint32_t serial,
                                       uint32_t time,
                                       uint32_t button,
                                       uint32_t state) = 0;

    virtual void notify_pointer_axis(Pointer *pointer,
                                     struct wl_pointer *wl_pointer,
                                     uint32_t time,
                                     uint32_t axis,
                                     wl_fixed_t value) = 0;

    virtual void notify_pointer_frame(Pointer *pointer,
                                      struct wl_pointer *wl_pointer) = 0;

    virtual void notify_pointer_axis_source(Pointer *pointer,
                                            struct wl_pointer *wl_pointer,
                                            uint32_t axis_source) = 0;

    virtual void notify_pointer_axis_stop(Pointer *pointer,
                                          struct wl_pointer *wl_pointer,
                                          uint32_t
                                          time,
                                          uint32_t axis) = 0;

    virtual void notify_pointer_axis_discrete(Pointer *pointer,
                                              struct wl_pointer *wl_pointer,
                                              uint32_t axis,
                                              int32_t discrete) = 0;
};

class Pointer {
public:
    explicit Pointer(struct wl_pointer *pointer, struct wl_compositor *wl_compositor,
                     const std::optional<struct wl_shm *> &wl_shm,
                     bool disable_cursor,
                     int size = 24);

    ~Pointer();

    void register_observer(PointerObserver *observer) {
        observers_.push_back(observer);
    }

    void unregister_observer(PointerObserver *observer) {
        observers_.remove(observer);
    }

    static std::string get_cursor_theme();

    static std::vector<std::string> get_available_cursors(const char* theme_name = nullptr);

    void set_cursor(uint32_t serial, const char *cursor_name = "right_ptr", const char *theme_name = nullptr);

    [[nodiscard]] bool is_cursor_enabled() const { return !disable_cursor_; }

    // Disallow copy and assign.
    Pointer(const Pointer &) = delete;

    Pointer &operator=(const Pointer &) = delete;

private:
    struct wl_pointer *wl_pointer_;
    std::list<PointerObserver *> observers_{};
    struct wl_surface *wl_surface_cursor_;
    struct wl_cursor_theme *theme_{};
    const std::optional<struct wl_shm *> &wl_shm_;
    bool disable_cursor_;
    int size_;

    static void handle_enter(void *data,
                             struct wl_pointer *pointer,
                             uint32_t serial,
                             struct wl_surface *surface,
                             wl_fixed_t sx,
                             wl_fixed_t sy);

    static void handle_leave(void *data,
                             struct wl_pointer *pointer,
                             uint32_t serial,
                             struct wl_surface *surface);

    static void handle_motion(void *data,
                              struct wl_pointer *pointer,
                              uint32_t time,
                              wl_fixed_t sx,
                              wl_fixed_t sy);

    static void handle_button(void *data,
                              struct wl_pointer *wl_pointer,
                              uint32_t serial,
                              uint32_t time,
                              uint32_t button,
                              uint32_t state);

    static void handle_axis(void *data,
                            struct wl_pointer *wl_pointer,
                            uint32_t time,
                            uint32_t axis,
                            wl_fixed_t value);

    static void handle_frame(void *data,
                             struct wl_pointer *wl_pointer);

    static void handle_axis_source(void *data,
                                   struct wl_pointer *wl_pointer,
                                   uint32_t axis_source);

    static void handle_axis_stop(void *data,
                                 struct wl_pointer *wl_pointer,
                                 uint32_t time,
                                 uint32_t axis);

    static void handle_axis_discrete(void *data,
                                     struct wl_pointer *wl_pointer,
                                     uint32_t axis,
                                     int32_t discrete);

    static constexpr struct wl_pointer_listener pointer_listener_ = {
            .enter = handle_enter,
            .leave = handle_leave,
            .motion = handle_motion,
            .button = handle_button,
            .axis = handle_axis,
            .frame = handle_frame,
            .axis_source = handle_axis_source,
            .axis_stop = handle_axis_stop,
            .axis_discrete = handle_axis_discrete,
    };
};
