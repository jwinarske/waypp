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

#include <wayland-client.h>
#include <string>

#include "waypp/window/csd_frame_extents.h"
#include "waypp/window/csd_hit_zone.h"

/// Abstract base class for CSD decoration renderers.
///
/// Lifecycle (called by CsdFrame in this order):
///   1. init()      — create wl_subsurface panels anchored to the parent
///   2. update()    — whenever title or activation state changes
///   3. resize()    — on every xdg_toplevel configure; returns new extents
///   4. commit()    — after xdg_surface_ack_configure; flush subsurface damage
///   5. destroy()   — tear down all Wayland objects created in init()
///
/// The built-in implementation is CsdShmPlugin (pure-SHM, no extra deps).
/// An optional CsdCairoPlugin can be added later without API changes.
class CsdPlugin {
 public:
  virtual ~CsdPlugin() = default;

  // Disallow copy and assign — plugins hold Wayland object ownership.
  CsdPlugin(const CsdPlugin&) = delete;
  CsdPlugin& operator=(const CsdPlugin&) = delete;

  /// One-time setup: create wl_subsurfaces anchored to @p parent_surface.
  ///
  /// @param compositor     wl_compositor for creating sub-surfaces.
  /// @param subcompositor  wl_subcompositor for wl_subsurface creation.
  /// @param shm            wl_shm for allocating SHM pixel buffers.
  /// @param parent_surface The toplevel wl_surface the panels attach to.
  /// @return true on success; false if any Wayland object allocation failed.
  virtual bool init(wl_compositor* compositor,
                    wl_subcompositor* subcompositor,
                    wl_shm* shm,
                    wl_surface* parent_surface) = 0;

  /// Notification that the window title or activation state has changed.
  ///
  /// The plugin should redraw the title bar on the next commit() call.
  /// This is called before resize() when both change simultaneously.
  ///
  /// @param title   UTF-8 window title string.
  /// @param active  True when the toplevel has keyboard focus.
  virtual void update(const std::string& title, bool active) = 0;

  /// Called on every xdg_toplevel::configure event with the *content* size
  /// (i.e. the compositor-provided dimensions, excluding any borders).
  ///
  /// The plugin must resize its subsurface pixel buffers to match the new
  /// content dimensions and return the border extents it claims.  The extents
  /// must be stable within a single configure/commit cycle.
  ///
  /// @param content_w  New content width in logical pixels (> 0).
  /// @param content_h  New content height in logical pixels (> 0).
  /// @return The border widths the plugin requires around the content area.
  ///         Returns {0,0,0,0} when decorations are hidden (e.g. fullscreen).
  virtual CsdFrameExtents resize(int32_t content_w, int32_t content_h) = 0;

  /// Commit subsurface damage after xdg_surface_ack_configure.
  ///
  /// Called by CsdFrame immediately after ack_configure, so subsurface
  /// positions and buffer damage are applied atomically with the content
  /// surface commit.
  virtual void commit() = 0;

  /// Hit-test: returns the decoration zone under the pointer at (@p x, @p y).
  ///
  /// @param surface  The wl_surface that received the pointer event.  Maybe
  ///                 the parent surface or one of the plugin's subsurfaces.
  /// @param x        Pointer X coordinate in surface-local logical pixels.
  /// @param y        Pointer Y coordinate in surface-local logical pixels.
  /// @return CsdHitZone::kNone when the pointer is over the content area, so
  ///         the event should be forwarded to the application.
  [[nodiscard]] virtual CsdHitZone hit_test(wl_surface* surface,
                                            double x,
                                            double y) const = 0;

  /// Show or hide all decoration subsurfaces.
  ///
  /// Called by CsdFrame when the window enters or leaves a state where
  /// decorations should not be shown (fullscreen, maximized, tiled).
  /// A hidden plugin must still return {0,0,0,0} from resize() and pass
  /// all pointer events through as CsdHitZone::kNone.
  virtual void set_visible(bool visible) = 0;

  /// Tear down all Wayland objects created during init().
  ///
  /// Called by CsdFrame before destruction.  After destroy() returns, the
  /// plugin must not access any Wayland objects.  init() will not be called
  /// again after destroy().
  virtual void destroy() = 0;

 protected:
  CsdPlugin() = default;
};
