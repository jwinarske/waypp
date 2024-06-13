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

#include <cstdint>
#include <string>

#include <waypp/waypp.h>

#include "window_manager.h"

class IviWindowManager : public WindowManager {
public:
    IviWindowManager(
            const char *title,
            const char *app_id,
            bool fullscreen,
            bool maximized,
            unsigned long ext_interface_count = 0,
            const Registrar::RegistrarCallback *ext_interface_data = nullptr,
            GMainContext *context = nullptr,
            const char *name = nullptr);

    ~IviWindowManager();

    int dispatch_pending() { return wl_display_dispatch_pending(get_display()); }

    // Disallow copy and assign.
    IviWindowManager(const IviWindowManager &) = delete;

    IviWindowManager &operator=(const IviWindowManager &) = delete;

private:
    struct ivi_wm *ivi_wm_;
    std::string app_title_;
    std::string app_id_;

    struct {
        int32_t width;
        int32_t height;
    } geometry_{};

    struct {
        int32_t width;
        int32_t height;
    } window_size_{};

    static void ivi_wm_surface_visibility(void *data,
                                          struct ivi_wm *ivi_wm,
                                          uint32_t surface_id,
                                          int32_t visibility);

    static void ivi_wm_layer_visibility(void *data,
                                        struct ivi_wm *ivi_wm,
                                        uint32_t layer_id,
                                        int32_t visibility);

    static void ivi_wm_surface_opacity(void *data,
                                       struct ivi_wm *ivi_wm,
                                       uint32_t surface_id,
                                       wl_fixed_t opacity);

    static void ivi_wm_layer_opacity(void *data,
                                     struct ivi_wm *ivi_wm,
                                     uint32_t layer_id,
                                     wl_fixed_t opacity);

    static void ivi_wm_surface_source_rectangle(void *data,
                                                struct ivi_wm *ivi_wm,
                                                uint32_t surface_id,
                                                int32_t x,
                                                int32_t y,
                                                int32_t width,
                                                int32_t height);

    static void ivi_wm_layer_source_rectangle(void *data,
                                              struct ivi_wm *ivi_wm,
                                              uint32_t layer_id,
                                              int32_t x,
                                              int32_t y,
                                              int32_t width,
                                              int32_t height);

    static void ivi_wm_surface_destination_rectangle(void *data,
                                                     struct ivi_wm *ivi_wm,
                                                     uint32_t surface_id,
                                                     int32_t x,
                                                     int32_t y,
                                                     int32_t width,
                                                     int32_t height);

    static void ivi_wm_layer_destination_rectangle(void *data,
                                                   struct ivi_wm *ivi_wm,
                                                   uint32_t layer_id,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height);

    static void ivi_wm_surface_created(void *data,
                                       struct ivi_wm *ivi_wm,
                                       uint32_t surface_id);

    static void ivi_wm_layer_created(void *data,
                                     struct ivi_wm *ivi_wm,
                                     uint32_t layer_id);

    static void ivi_wm_surface_destroyed(void *data,
                                         struct ivi_wm *ivi_wm,
                                         uint32_t surface_id);

    static void ivi_wm_layer_destroyed(void *data,
                                       struct ivi_wm *ivi_wm,
                                       uint32_t layer_id);

    static void ivi_wm_surface_error(void *data,
                                     struct ivi_wm *ivi_wm,
                                     uint32_t object_id,
                                     uint32_t error,
                                     const char *message);

    static void ivi_wm_layer_error(void *data,
                                   struct ivi_wm *ivi_wm,
                                   uint32_t object_id,
                                   uint32_t error,
                                   const char *message);

    static void ivi_wm_surface_size(void * /* data */,
                                    struct ivi_wm * /* ivi_wm */,
                                    uint32_t surface_id,
                                    int32_t width,
                                    int32_t height);

    static void ivi_wm_surface_stats(void * /* data */,
                                     struct ivi_wm * /* ivi_wm */,
                                     uint32_t surface_id,
                                     uint32_t frame_count,
                                     uint32_t pid);

    static void ivi_wm_layer_surface_added(void *data,
                                           struct ivi_wm *ivi_wm,
                                           uint32_t layer_id,
                                           uint32_t surface_id);

    static constexpr struct ivi_wm_listener ivi_wm_listener_ = {
            .surface_visibility = ivi_wm_surface_visibility,
            .layer_visibility = ivi_wm_layer_visibility,
            .surface_opacity = ivi_wm_surface_opacity,
            .layer_opacity = ivi_wm_layer_opacity,
            .surface_source_rectangle = ivi_wm_surface_source_rectangle,
            .layer_source_rectangle = ivi_wm_layer_source_rectangle,
            .surface_destination_rectangle = ivi_wm_surface_destination_rectangle,
            .layer_destination_rectangle = ivi_wm_layer_destination_rectangle,
            .surface_created = ivi_wm_surface_created,
            .layer_created = ivi_wm_layer_created,
            .surface_destroyed = ivi_wm_surface_destroyed,
            .layer_destroyed = ivi_wm_layer_destroyed,
            .surface_error = ivi_wm_surface_error,
            .layer_error = ivi_wm_layer_error,
            .surface_size = ivi_wm_surface_size,
            .surface_stats = ivi_wm_surface_stats,
            .layer_surface_added = ivi_wm_layer_surface_added,
    };
};
