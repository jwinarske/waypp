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

#pragma once

#include <waypp/waypp.h>

#if ENABLE_IVI_SHELL_CLIENT

#include <cstdint>

/**
 * @class IviWm
 *
 * @brief Optional ergonomic wrapper around the raw ivi_wm* handle.
 *
 * Not all IVI compositors advertise the ivi_wm global (it is a controller /
 * HMI-orchestration interface from ADIT). This class should only be
 * instantiated when Registrar::get_ivi_wm() returns a non-null pointer.
 *
 * All mutating requests (visibility, opacity, source/destination rectangles,
 * layer membership) are batched on the compositor side and only applied when
 * commit() is called, which maps to ivi_wm_commit_changes().  The exceptions
 * are create_layer() and destroy_layer() which take effect immediately.
 *
 * Compositor-pushed events (surface/layer created, destroyed, property
 * changes, errors) are logged at DEBUG level via DLOG_DEBUG.
 *
 * Usage:
 * @code
 *   auto* raw_wm = registrar.get_ivi_wm();          // may be nullptr
 *   if (raw_wm) {
 *     IviWm wm(raw_wm);
 *     wm.create_layer(1000, 1920, 1080);
 *     wm.layer_add_surface(1000, 9000);
 *     wm.set_layer_visibility(1000, true);
 *     wm.set_surface_visibility(9000, true);
 *     wm.commit();
 *   }
 * @endcode
 */
class IviWm {
 public:
  /**
   * @brief Construct from a non-null ivi_wm* obtained from Registrar.
   * @param wm  Raw ivi_wm handle; must not be null.
   */
  explicit IviWm(ivi_wm* wm);

  ~IviWm();

  // ── Layer management ────────────────────────────────────────────────────

  /**
   * @brief Create a new layout layer with the given ID and dimensions.
   *
   * Takes effect immediately (not deferred by commit()).
   *
   * @param layer_id  Unique layer identifier.
   * @param width     Layer width in pixels.
   * @param height    Layer height in pixels.
   */
  void create_layer(uint32_t layer_id, int width, int height);

  /**
   * @brief Destroy an existing layout layer.
   *
   * Takes effect immediately (not deferred by commit()).
   *
   * @param layer_id  ID of the layer to destroy.
   */
  void destroy_layer(uint32_t layer_id);

  /**
   * @brief Remove all surfaces from a layer's render order.
   * @param layer_id  Target layer.
   */
  void layer_clear(uint32_t layer_id);

  /**
   * @brief Add a surface to the topmost position of a layer's render order.
   * @param layer_id   Target layer.
   * @param surface_id IVI surface ID to add.
   */
  void layer_add_surface(uint32_t layer_id, uint32_t surface_id);

  /**
   * @brief Remove a surface from a layer's render order.
   *
   * The surface is not destroyed; it is merely de-listed from the layer.
   *
   * @param layer_id   Target layer.
   * @param surface_id IVI surface ID to remove.
   */
  void layer_remove_surface(uint32_t layer_id, uint32_t surface_id);

  // ── Visibility ──────────────────────────────────────────────────────────

  /**
   * @brief Set the visibility of a surface.
   * @param surface_id  Target IVI surface.
   * @param visible     true = visible, false = invisible.
   */
  void set_surface_visibility(uint32_t surface_id, bool visible);

  /**
   * @brief Set the visibility of a layer.
   * @param layer_id  Target layer.
   * @param visible   true = visible, false = invisible.
   */
  void set_layer_visibility(uint32_t layer_id, bool visible);

  // ── Opacity ─────────────────────────────────────────────────────────────

  /**
   * @brief Set surface opacity (0.0 = fully transparent, 1.0 = fully opaque).
   * @param surface_id  Target IVI surface.
   * @param opacity     Value in [0.0, 1.0].
   */
  void set_surface_opacity(uint32_t surface_id, double opacity);

  /**
   * @brief Set layer opacity (0.0 = fully transparent, 1.0 = fully opaque).
   * @param layer_id  Target layer.
   * @param opacity   Value in [0.0, 1.0].
   */
  void set_layer_opacity(uint32_t layer_id, double opacity);

  // ── Source rectangles ───────────────────────────────────────────────────

  /**
   * @brief Set the scanout (source) region of a surface.
   *
   * Defines the portion of the surface buffer used for compositing.
   * Pass -1 for any parameter to leave it unchanged.
   */
  void set_surface_source_rectangle(uint32_t surface_id,
                                    int x,
                                    int y,
                                    int width,
                                    int height);

  /**
   * @brief Set the scanout (source) region of a layer.
   *
   * Pass -1 for any parameter to leave it unchanged.
   */
  void set_layer_source_rectangle(uint32_t layer_id,
                                  int x,
                                  int y,
                                  int width,
                                  int height);

  // ── Destination rectangles ──────────────────────────────────────────────

  /**
   * @brief Set the destination (position + size) of a surface within its layer.
   *
   * The surface content is scaled to fill this rectangle.
   * Pass -1 for any parameter to leave it unchanged.
   */
  void set_surface_destination(uint32_t surface_id,
                               int x,
                               int y,
                               int width,
                               int height);

  /**
   * @brief Set the destination (position + size) of a layer within its screen.
   *
   * Pass -1 for any parameter to leave it unchanged.
   */
  void set_layer_destination(uint32_t layer_id,
                             int x,
                             int y,
                             int width,
                             int height);

  // ── Surface type ────────────────────────────────────────────────────────

  /**
   * @brief Set the surface type (restricted vs desktop-compatible).
   *
   * Desktop-compatible surfaces allow the compositor to adjust source and
   * destination regions when the application resizes its buffers.
   *
   * @param surface_id  Target IVI surface.
   * @param type        IVI_WM_SURFACE_TYPE_RESTRICTED (0) or
   *                    IVI_WM_SURFACE_TYPE_DESKTOP (1).
   */
  void set_surface_type(uint32_t surface_id, int type);

  // ── Sync / get ──────────────────────────────────────────────────────────

  /**
   * @brief Request continuous property events for a surface.
   * @param surface_id  Target IVI surface.
   * @param sync_state  IVI_WM_SYNC_ADD (0) to start, IVI_WM_SYNC_REMOVE (1)
   *                    to stop.
   */
  void surface_sync(uint32_t surface_id, int sync_state);

  /**
   * @brief Request continuous property events for a layer.
   * @param layer_id    Target layer.
   * @param sync_state  IVI_WM_SYNC_ADD (0) to start, IVI_WM_SYNC_REMOVE (1)
   *                    to stop.
   */
  void layer_sync(uint32_t layer_id, int sync_state);

  // ── Commit ──────────────────────────────────────────────────────────────

  /**
   * @brief Apply all pending property changes to the scene.
   *
   * All set_* and layer_* requests are batched until this call.
   * Equivalent to ivi_wm_commit_changes().
   */
  void commit();

  // Disallow copy and assign.
  IviWm(const IviWm&) = delete;
  IviWm& operator=(const IviWm&) = delete;

 private:
  ivi_wm* ivi_wm_{};

  static const ivi_wm_listener ivi_wm_listener_;

  // ── Compositor-pushed event handlers (all logged at DEBUG level) ─────────

  static void handle_surface_visibility(void* data,
                                        ivi_wm* wm,
                                        uint32_t surface_id,
                                        int32_t visibility);

  static void handle_layer_visibility(void* data,
                                      ivi_wm* wm,
                                      uint32_t layer_id,
                                      int32_t visibility);

  static void handle_surface_opacity(void* data,
                                     ivi_wm* wm,
                                     uint32_t surface_id,
                                     wl_fixed_t opacity);

  static void handle_layer_opacity(void* data,
                                   ivi_wm* wm,
                                   uint32_t layer_id,
                                   wl_fixed_t opacity);

  static void handle_surface_source_rectangle(void* data,
                                              ivi_wm* wm,
                                              uint32_t surface_id,
                                              int32_t x,
                                              int32_t y,
                                              int32_t width,
                                              int32_t height);

  static void handle_layer_source_rectangle(void* data,
                                            ivi_wm* wm,
                                            uint32_t layer_id,
                                            int32_t x,
                                            int32_t y,
                                            int32_t width,
                                            int32_t height);

  static void handle_surface_destination_rectangle(void* data,
                                                   ivi_wm* wm,
                                                   uint32_t surface_id,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height);

  static void handle_layer_destination_rectangle(void* data,
                                                 ivi_wm* wm,
                                                 uint32_t layer_id,
                                                 int32_t x,
                                                 int32_t y,
                                                 int32_t width,
                                                 int32_t height);

  static void handle_surface_created(void* data,
                                     ivi_wm* wm,
                                     uint32_t surface_id);

  static void handle_layer_created(void* data,
                                   ivi_wm* wm,
                                   uint32_t layer_id);

  static void handle_surface_destroyed(void* data,
                                       ivi_wm* wm,
                                       uint32_t surface_id);

  static void handle_layer_destroyed(void* data,
                                     ivi_wm* wm,
                                     uint32_t layer_id);

  static void handle_surface_error(void* data,
                                   ivi_wm* wm,
                                   uint32_t object_id,
                                   uint32_t error,
                                   const char* message);

  static void handle_layer_error(void* data,
                                 ivi_wm* wm,
                                 uint32_t object_id,
                                 uint32_t error,
                                 const char* message);

  static void handle_surface_size(void* data,
                                  ivi_wm* wm,
                                  uint32_t surface_id,
                                  int32_t width,
                                  int32_t height);

  static void handle_surface_stats(void* data,
                                   ivi_wm* wm,
                                   uint32_t surface_id,
                                   uint32_t frame_count,
                                   uint32_t pid);

  static void handle_layer_surface_added(void* data,
                                         ivi_wm* wm,
                                         uint32_t layer_id,
                                         uint32_t surface_id);
};

#endif  // ENABLE_IVI_SHELL_CLIENT

