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

#include "ivi_window_manager.h"

#include "logging.h"

IviWindowManager::IviWindowManager(const char *title,
                                   const char *app_id,
                                   bool /* fullscreen */,
                                   bool /* maximized */,
                                   const unsigned long ext_interface_count,
                                   const Registrar::RegistrarCallback *ext_interface_data,
                                   GMainContext *context,
                                   bool enable_cursor,
                                   const char *name) : WindowManager(ext_interface_count,
                                                                     ext_interface_data,
                                                                     context, enable_cursor,
                                                                     name), app_title_(title),
                                                       app_id_(app_id) {
    auto ivi_wm = get_ivi_wm();
    if (!ivi_wm.has_value()) {
        spdlog::critical("{} is not present.", ivi_wm_interface.name);
        exit(EXIT_FAILURE);
    }
    ivi_wm_ = ivi_wm.value();
}

IviWindowManager::~IviWindowManager() = default;

void IviWindowManager::ivi_wm_surface_visibility(void *data,
                                                 struct ivi_wm *ivi_wm,
                                                 uint32_t surface_id,
                                                 int32_t visibility) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) surface_id;
    (void) visibility;
    SPDLOG_DEBUG("ivi_wm_surface_visibility: {}, visibility: {}", surface_id,
                 visibility);
}

void IviWindowManager::ivi_wm_layer_visibility(void *data,
                                               struct ivi_wm *ivi_wm,
                                               uint32_t layer_id,
                                               int32_t visibility) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) layer_id;
    (void) visibility;
    SPDLOG_DEBUG("ivi_wm_layer_visibility: {}, visibility: {}", layer_id,
                 visibility);
}

void IviWindowManager::ivi_wm_surface_opacity(void *data,
                                              struct ivi_wm *ivi_wm,
                                              uint32_t surface_id,
                                              wl_fixed_t opacity) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) surface_id;
    (void) opacity;
    SPDLOG_DEBUG("ivi_wm_surface_opacity: {}, opacity: {}", surface_id, opacity);
}

void IviWindowManager::ivi_wm_layer_opacity(void *data,
                                            struct ivi_wm *ivi_wm,
                                            uint32_t layer_id,
                                            wl_fixed_t opacity) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) layer_id;
    (void) opacity;
    SPDLOG_DEBUG("ivi_wm_layer_opacity: {}, opacity: {}", layer_id, opacity);
}

void IviWindowManager::ivi_wm_surface_source_rectangle(void *data,
                                                       struct ivi_wm *ivi_wm,
                                                       uint32_t surface_id,
                                                       int32_t x,
                                                       int32_t y,
                                                       int32_t width,
                                                       int32_t height) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) surface_id;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    SPDLOG_DEBUG(
            "ivi_wm_surface_source_rectangle: {}, x: {}, y: {}, width: {}, height: "
            "{}",
            surface_id, x, y, width, height);
}

void IviWindowManager::ivi_wm_layer_source_rectangle(void *data,
                                                     struct ivi_wm *ivi_wm,
                                                     uint32_t layer_id,
                                                     int32_t x,
                                                     int32_t y,
                                                     int32_t width,
                                                     int32_t height) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) layer_id;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    SPDLOG_DEBUG(
            "ivi_wm_layer_source_rectangle: {}, x: {}, y: {}, width: {}, height: {}",
            layer_id, x, y, width, height);
}

void IviWindowManager::ivi_wm_surface_destination_rectangle(void *data,
                                                            struct ivi_wm *ivi_wm,
                                                            uint32_t surface_id,
                                                            int32_t x,
                                                            int32_t y,
                                                            int32_t width,
                                                            int32_t height) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) surface_id;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    SPDLOG_DEBUG(
            "ivi_wm_surface_destination_rectangle: {}, x: {}, y: {}, width: {}, "
            "height: {}",
            surface_id, x, y, width, height);
}

void IviWindowManager::ivi_wm_layer_destination_rectangle(void *data,
                                                          struct ivi_wm *ivi_wm,
                                                          uint32_t layer_id,
                                                          int32_t x,
                                                          int32_t y,
                                                          int32_t width,
                                                          int32_t height) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) layer_id;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    SPDLOG_DEBUG(
            "ivi_wm_surface_destination_rectangle: {}, , x: {}, y: {}, width: {}, "
            "height: {}",
            layer_id, x, y, width, height);
}

void IviWindowManager::ivi_wm_surface_created(void *data,
                                              struct ivi_wm *ivi_wm,
                                              uint32_t surface_id) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) surface_id;
    SPDLOG_DEBUG("ivi_wm_surface_created: {}", surface_id);
}

void IviWindowManager::ivi_wm_layer_created(void *data,
                                            struct ivi_wm *ivi_wm,
                                            uint32_t layer_id) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) layer_id;
    SPDLOG_DEBUG("ivi_wm_layer_created: {}", layer_id);
}

void IviWindowManager::ivi_wm_surface_destroyed(void *data,
                                                struct ivi_wm *ivi_wm,
                                                uint32_t surface_id) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) surface_id;
    SPDLOG_DEBUG("ivi_wm_surface_destroyed: {}", surface_id);
}

void IviWindowManager::ivi_wm_layer_destroyed(void *data,
                                              struct ivi_wm *ivi_wm,
                                              uint32_t layer_id) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) layer_id;
    SPDLOG_DEBUG("ivi_wm_layer_destroyed: {}", layer_id);
}

void IviWindowManager::ivi_wm_surface_error(void *data,
                                            struct ivi_wm *ivi_wm,
                                            uint32_t object_id,
                                            uint32_t error,
                                            const char *message) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) object_id;
    (void) error;
    (void) message;
    SPDLOG_DEBUG("ivi_wm_surface_error: {}, error ({}), {}", object_id, error,
                 message);
}

void IviWindowManager::ivi_wm_layer_error(void *data,
                                          struct ivi_wm *ivi_wm,
                                          uint32_t object_id,
                                          uint32_t error,
                                          const char *message) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) object_id;
    (void) error;
    (void) message;
    SPDLOG_DEBUG("ivi_wm_layer_error: {}, error ({}), {}", object_id, error,
                 message);
}

void IviWindowManager::ivi_wm_surface_size(void *data,
                                           struct ivi_wm *ivi_wm,
                                           uint32_t surface_id,
                                           int32_t width,
                                           int32_t height) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) surface_id;
    (void) width;
    (void) height;
    SPDLOG_DEBUG("ivi_wm_surface_size: {}, width {}, height {}", surface_id,
                 width, height);
}

void IviWindowManager::ivi_wm_surface_stats(void *data,
                                            struct ivi_wm *ivi_wm,
                                            uint32_t surface_id,
                                            uint32_t frame_count,
                                            uint32_t pid) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) surface_id;
    (void) frame_count;
    (void) pid;
    SPDLOG_DEBUG("ivi_wm_surface_stats: {}, frame_count {}, pid {}", surface_id,
                 frame_count, pid);
}

void IviWindowManager::ivi_wm_layer_surface_added(void *data,
                                                  struct ivi_wm *ivi_wm,
                                                  uint32_t layer_id,
                                                  uint32_t surface_id) {
    auto obj = static_cast<IviWindowManager *>(data);
    if (obj->ivi_wm_ != ivi_wm) {
        return;
    }
    (void) layer_id;
    (void) surface_id;
    SPDLOG_DEBUG("ivi_wm_layer_surface_added: {}, {}", layer_id, surface_id);
}
