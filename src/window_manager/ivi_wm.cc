/*
 * Copyright 2026 Joel Winarske
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

#include "waypp/window_manager/ivi_wm.h"

#if ENABLE_IVI_SHELL_CLIENT

#include "logging/logging.h"

// ── Static listener vtable ─────────────────────────────────────────────────

const ivi_wm_listener IviWm::ivi_wm_listener_ = {
    .surface_visibility = handle_surface_visibility,
    .layer_visibility = handle_layer_visibility,
    .surface_opacity = handle_surface_opacity,
    .layer_opacity = handle_layer_opacity,
    .surface_source_rectangle = handle_surface_source_rectangle,
    .layer_source_rectangle = handle_layer_source_rectangle,
    .surface_destination_rectangle = handle_surface_destination_rectangle,
    .layer_destination_rectangle = handle_layer_destination_rectangle,
    .surface_created = handle_surface_created,
    .layer_created = handle_layer_created,
    .surface_destroyed = handle_surface_destroyed,
    .layer_destroyed = handle_layer_destroyed,
    .surface_error = handle_surface_error,
    .layer_error = handle_layer_error,
    .surface_size = handle_surface_size,
    .surface_stats = handle_surface_stats,
    .layer_surface_added = handle_layer_surface_added,
};

// ── Construction / destruction ─────────────────────────────────────────────

IviWm::IviWm(ivi_wm* wm) : ivi_wm_(wm) {
  DLOG_TRACE("++IviWm::IviWm()");
  ivi_wm_add_listener(ivi_wm_, &ivi_wm_listener_, this);
  DLOG_TRACE("--IviWm::IviWm()");
}

IviWm::~IviWm() {
  if (ivi_wm_) {
    DLOG_TRACE("[IviWm] ivi_wm_destroy()");
    ivi_wm_destroy(ivi_wm_);
    ivi_wm_ = nullptr;
  }
}

// ── Layer management ──────────────────────────────────────────────────────

void IviWm::create_layer(const uint32_t layer_id,
                         const int width,
                         const int height) {
  DLOG_DEBUG("[IviWm] create_layer layer_id={} size={}x{}", layer_id, width,
             height);
  ivi_wm_create_layout_layer(ivi_wm_, layer_id, width, height);
}

void IviWm::destroy_layer(const uint32_t layer_id) {
  DLOG_DEBUG("[IviWm] destroy_layer layer_id={}", layer_id);
  ivi_wm_destroy_layout_layer(ivi_wm_, layer_id);
}

void IviWm::layer_clear(const uint32_t layer_id) {
  DLOG_DEBUG("[IviWm] layer_clear layer_id={}", layer_id);
  ivi_wm_layer_clear(ivi_wm_, layer_id);
}

void IviWm::layer_add_surface(const uint32_t layer_id,
                              const uint32_t surface_id) {
  DLOG_DEBUG("[IviWm] layer_add_surface layer_id={} surface_id={}", layer_id,
             surface_id);
  ivi_wm_layer_add_surface(ivi_wm_, layer_id, surface_id);
}

void IviWm::layer_remove_surface(const uint32_t layer_id,
                                 const uint32_t surface_id) {
  DLOG_DEBUG("[IviWm] layer_remove_surface layer_id={} surface_id={}",
             layer_id, surface_id);
  ivi_wm_layer_remove_surface(ivi_wm_, layer_id, surface_id);
}

// ── Visibility ────────────────────────────────────────────────────────────

void IviWm::set_surface_visibility(const uint32_t surface_id,
                                   const bool visible) {
  DLOG_DEBUG("[IviWm] set_surface_visibility surface_id={} visible={}",
             surface_id, visible);
  ivi_wm_set_surface_visibility(ivi_wm_, surface_id,
                                static_cast<uint32_t>(visible));
}

void IviWm::set_layer_visibility(const uint32_t layer_id, const bool visible) {
  DLOG_DEBUG("[IviWm] set_layer_visibility layer_id={} visible={}", layer_id,
             visible);
  ivi_wm_set_layer_visibility(ivi_wm_, layer_id,
                              static_cast<uint32_t>(visible));
}

// ── Opacity ───────────────────────────────────────────────────────────────

void IviWm::set_surface_opacity(const uint32_t surface_id,
                                const double opacity) {
  DLOG_DEBUG("[IviWm] set_surface_opacity surface_id={} opacity={}", surface_id,
             opacity);
  ivi_wm_set_surface_opacity(ivi_wm_, surface_id,
                             wl_fixed_from_double(opacity));
}

void IviWm::set_layer_opacity(const uint32_t layer_id, const double opacity) {
  DLOG_DEBUG("[IviWm] set_layer_opacity layer_id={} opacity={}", layer_id,
             opacity);
  ivi_wm_set_layer_opacity(ivi_wm_, layer_id, wl_fixed_from_double(opacity));
}

// ── Source rectangles ─────────────────────────────────────────────────────

void IviWm::set_surface_source_rectangle(const uint32_t surface_id,
                                         const int x,
                                         const int y,
                                         const int width,
                                         const int height) {
  DLOG_DEBUG(
      "[IviWm] set_surface_source_rectangle surface_id={} x={} y={} w={} h={}",
      surface_id, x, y, width, height);
  ivi_wm_set_surface_source_rectangle(ivi_wm_, surface_id, x, y, width,
                                      height);
}

void IviWm::set_layer_source_rectangle(const uint32_t layer_id,
                                       const int x,
                                       const int y,
                                       const int width,
                                       const int height) {
  DLOG_DEBUG(
      "[IviWm] set_layer_source_rectangle layer_id={} x={} y={} w={} h={}",
      layer_id, x, y, width, height);
  ivi_wm_set_layer_source_rectangle(ivi_wm_, layer_id, x, y, width, height);
}

// ── Destination rectangles ────────────────────────────────────────────────

void IviWm::set_surface_destination(const uint32_t surface_id,
                                    const int x,
                                    const int y,
                                    const int width,
                                    const int height) {
  DLOG_DEBUG(
      "[IviWm] set_surface_destination surface_id={} x={} y={} w={} h={}",
      surface_id, x, y, width, height);
  ivi_wm_set_surface_destination_rectangle(ivi_wm_, surface_id, x, y, width,
                                           height);
}

void IviWm::set_layer_destination(const uint32_t layer_id,
                                  const int x,
                                  const int y,
                                  const int width,
                                  const int height) {
  DLOG_DEBUG(
      "[IviWm] set_layer_destination layer_id={} x={} y={} w={} h={}",
      layer_id, x, y, width, height);
  ivi_wm_set_layer_destination_rectangle(ivi_wm_, layer_id, x, y, width,
                                         height);
}

// ── Surface type ──────────────────────────────────────────────────────────

void IviWm::set_surface_type(const uint32_t surface_id, const int type) {
  DLOG_DEBUG("[IviWm] set_surface_type surface_id={} type={}", surface_id,
             type);
  ivi_wm_set_surface_type(ivi_wm_, surface_id, type);
}

// ── Sync / get ────────────────────────────────────────────────────────────

void IviWm::surface_sync(const uint32_t surface_id, const int sync_state) {
  DLOG_DEBUG("[IviWm] surface_sync surface_id={} sync_state={}", surface_id,
             sync_state);
  ivi_wm_surface_sync(ivi_wm_, surface_id, sync_state);
}

void IviWm::layer_sync(const uint32_t layer_id, const int sync_state) {
  DLOG_DEBUG("[IviWm] layer_sync layer_id={} sync_state={}", layer_id,
             sync_state);
  ivi_wm_layer_sync(ivi_wm_, layer_id, sync_state);
}

// ── Commit ────────────────────────────────────────────────────────────────

void IviWm::commit() {
  DLOG_DEBUG("[IviWm] commit_changes");
  ivi_wm_commit_changes(ivi_wm_);
}

// ── Compositor-pushed event handlers ─────────────────────────────────────

void IviWm::handle_surface_visibility(void* /*data*/,
                                      ivi_wm* /*wm*/,
                                      const uint32_t surface_id,
                                      const int32_t visibility) {
  DLOG_DEBUG("[IviWm] surface_visibility surface_id={} visibility={}",
             surface_id, visibility);
}

void IviWm::handle_layer_visibility(void* /*data*/,
                                    ivi_wm* /*wm*/,
                                    const uint32_t layer_id,
                                    const int32_t visibility) {
  DLOG_DEBUG("[IviWm] layer_visibility layer_id={} visibility={}", layer_id,
             visibility);
}

void IviWm::handle_surface_opacity(void* /*data*/,
                                   ivi_wm* /*wm*/,
                                   const uint32_t surface_id,
                                   const wl_fixed_t opacity) {
  DLOG_DEBUG("[IviWm] surface_opacity surface_id={} opacity={}", surface_id,
             wl_fixed_to_double(opacity));
}

void IviWm::handle_layer_opacity(void* /*data*/,
                                 ivi_wm* /*wm*/,
                                 const uint32_t layer_id,
                                 const wl_fixed_t opacity) {
  DLOG_DEBUG("[IviWm] layer_opacity layer_id={} opacity={}", layer_id,
             wl_fixed_to_double(opacity));
}

void IviWm::handle_surface_source_rectangle(void* /*data*/,
                                            ivi_wm* /*wm*/,
                                            const uint32_t surface_id,
                                            const int32_t x,
                                            const int32_t y,
                                            const int32_t width,
                                            const int32_t height) {
  DLOG_DEBUG(
      "[IviWm] surface_source_rectangle surface_id={} x={} y={} w={} h={}",
      surface_id, x, y, width, height);
}

void IviWm::handle_layer_source_rectangle(void* /*data*/,
                                          ivi_wm* /*wm*/,
                                          const uint32_t layer_id,
                                          const int32_t x,
                                          const int32_t y,
                                          const int32_t width,
                                          const int32_t height) {
  DLOG_DEBUG(
      "[IviWm] layer_source_rectangle layer_id={} x={} y={} w={} h={}",
      layer_id, x, y, width, height);
}

void IviWm::handle_surface_destination_rectangle(void* /*data*/,
                                                 ivi_wm* /*wm*/,
                                                 const uint32_t surface_id,
                                                 const int32_t x,
                                                 const int32_t y,
                                                 const int32_t width,
                                                 const int32_t height) {
  DLOG_DEBUG(
      "[IviWm] surface_destination_rectangle surface_id={} x={} y={} w={} "
      "h={}",
      surface_id, x, y, width, height);
}

void IviWm::handle_layer_destination_rectangle(void* /*data*/,
                                               ivi_wm* /*wm*/,
                                               const uint32_t layer_id,
                                               const int32_t x,
                                               const int32_t y,
                                               const int32_t width,
                                               const int32_t height) {
  DLOG_DEBUG(
      "[IviWm] layer_destination_rectangle layer_id={} x={} y={} w={} h={}",
      layer_id, x, y, width, height);
}

void IviWm::handle_surface_created(void* /*data*/,
                                   ivi_wm* /*wm*/,
                                   const uint32_t surface_id) {
  DLOG_DEBUG("[IviWm] surface_created surface_id={}", surface_id);
}

void IviWm::handle_layer_created(void* /*data*/,
                                 ivi_wm* /*wm*/,
                                 const uint32_t layer_id) {
  DLOG_DEBUG("[IviWm] layer_created layer_id={}", layer_id);
}

void IviWm::handle_surface_destroyed(void* /*data*/,
                                     ivi_wm* /*wm*/,
                                     const uint32_t surface_id) {
  DLOG_DEBUG("[IviWm] surface_destroyed surface_id={}", surface_id);
}

void IviWm::handle_layer_destroyed(void* /*data*/,
                                   ivi_wm* /*wm*/,
                                   const uint32_t layer_id) {
  DLOG_DEBUG("[IviWm] layer_destroyed layer_id={}", layer_id);
}

void IviWm::handle_surface_error(void* /*data*/,
                                 ivi_wm* /*wm*/,
                                 const uint32_t object_id,
                                 const uint32_t error,
                                 const char* message) {
  LOG_ERROR("[IviWm] surface_error object_id={} error={} message={}", object_id,
            error, message ? message : "(null)");
}

void IviWm::handle_layer_error(void* /*data*/,
                               ivi_wm* /*wm*/,
                               const uint32_t object_id,
                               const uint32_t error,
                               const char* message) {
  LOG_ERROR("[IviWm] layer_error object_id={} error={} message={}", object_id,
            error, message ? message : "(null)");
}

void IviWm::handle_surface_size(void* /*data*/,
                                ivi_wm* /*wm*/,
                                const uint32_t surface_id,
                                const int32_t width,
                                const int32_t height) {
  DLOG_DEBUG("[IviWm] surface_size surface_id={} size={}x{}", surface_id,
             width, height);
}

void IviWm::handle_surface_stats(void* /*data*/,
                                 ivi_wm* /*wm*/,
                                 const uint32_t surface_id,
                                 const uint32_t frame_count,
                                 const uint32_t pid) {
  DLOG_DEBUG("[IviWm] surface_stats surface_id={} frame_count={} pid={}",
             surface_id, frame_count, pid);
}

void IviWm::handle_layer_surface_added(void* /*data*/,
                                       ivi_wm* /*wm*/,
                                       const uint32_t layer_id,
                                       const uint32_t surface_id) {
  DLOG_DEBUG("[IviWm] layer_surface_added layer_id={} surface_id={}", layer_id,
             surface_id);
}

#endif  // ENABLE_IVI_SHELL_CLIENT

