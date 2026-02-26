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

#include <memory>
#include <string>

#include <wayland-client.h>

#include "waypp/seat/pointer.h"
#include "waypp/waypp.h"
#include "waypp/window/csd_frame_extents.h"
#include "waypp/window/csd_hit_zone.h"
#include "waypp/window/csd_plugin.h"

class WindowManager;
class XdgTopLevel;

/// Orchestrates client-side window decorations for a single XdgTopLevel.
///
/// CsdFrame:
///   - Negotiates CSD vs. SSD via zxdg_decoration_manager_v1 when available.
///   - Delegates all rendering to a CsdPlugin (default: CsdShmPlugin).
///   - Implements PointerObserver to intercept decoration pointer events,
///     update the cursor shape, and initiate interactive move / resize.
///   - Is owned by XdgTopLevel as an optional member; callers interact with
///     XdgTopLevel normally and CsdFrame integrates transparently.
///
/// Lifecycle:
///   CsdFrame::create()           — factory; negotiates SSD, inits plugin
///   on_configure(w, h, ...)      — called from handle_xdg_toplevel_configure
///   commit()                     — called from handle_xdg_surface_configure
///   ~CsdFrame()                  — calls plugin->destroy(), cleans up protocol
///   objects
class CsdFrame : public PointerObserver {
 public:
  /// Factory method.  Returns nullptr if plugin->init() fails.
  ///
  /// When zxdg_decoration_manager_v1 is available, CSD is requested and the
  /// compositor's preference is honored.  If the compositor mandates
  /// SERVER_SIDE, the plugin is never initialized and extents() returns {0}.
  ///
  /// @param toplevel  The owning toplevel (non-owning pointer, must outlive
  /// us).
  /// @param wm        Window manager providing Wayland globals.
  /// @param title     Initial window title forwarded to the plugin.
  /// @param plugin    Decoration renderer; nullptr → CsdShmPlugin is created.
  static std::unique_ptr<CsdFrame> create(
      XdgTopLevel* toplevel,
      WindowManager* wm,
      const std::string& title,
      std::unique_ptr<CsdPlugin> plugin = nullptr);

  ~CsdFrame() override;

  // Disallow copy and assign.
  CsdFrame(const CsdFrame&) = delete;
  CsdFrame& operator=(const CsdFrame&) = delete;

  /// Called by XdgTopLevel::handle_xdg_toplevel_configure.
  ///
  /// Forwards the content size to the plugin, stores the resulting extents,
  /// and hides decorations when the window is not in floating state.
  ///
  /// @param content_w   New content width  (compositor-provided, > 0).
  /// @param content_h   New content height (compositor-provided, > 0).
  /// @param active      True when the toplevel holds keyboard focus.
  /// @param fullscreen  True when XDG_TOPLEVEL_STATE_FULLSCREEN is set.
  /// @param maximized   True when XDG_TOPLEVEL_STATE_MAXIMIZED is set.
  /// @param tiled       True when any XDG_TOPLEVEL_STATE_TILED_* is set.
  void on_configure(int32_t content_w,
                    int32_t content_h,
                    bool active,
                    bool fullscreen,
                    bool maximized,
                    bool tiled);

  /// Called by XdgTopLevel::handle_xdg_surface_configure after ack_configure.
  ///
  /// Delegates to plugin->commit() so subsurface positions and buffer damage
  /// are applied atomically with the content surface commit.
  void commit() const;

  /// Current decoration border widths in logical pixels.
  /// Returns {0,0,0,0} when SSD is active, decorations are hidden, or CSD
  /// is disabled — callers may use this unconditionally without special-casing.
  [[nodiscard]] const CsdFrameExtents& extents() const { return extents_; }

  /// Show or hide all decoration subsurfaces.
  /// Automatically called by on_configure() based on the window state.
  void set_visible(bool visible);

  /// Update the window title shown in the decoration title bar.
  void set_title(const std::string& title);

  // -------------------------------------------------------------------------
  // PointerObserver — registered with the seat Pointer by CsdFrame::create().
  // -------------------------------------------------------------------------

  void notify_pointer_enter(Pointer* pointer,
                            wl_pointer* wl_pointer,
                            uint32_t serial,
                            wl_surface* surface,
                            double sx,
                            double sy) override;

  void notify_pointer_leave(Pointer* pointer,
                            wl_pointer* wl_pointer,
                            uint32_t serial,
                            wl_surface* surface) override;

  void notify_pointer_motion(Pointer* pointer,
                             wl_pointer* wl_pointer,
                             uint32_t time,
                             double sx,
                             double sy) override;

  void notify_pointer_button(Pointer* pointer,
                             wl_pointer* wl_pointer,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t button,
                             uint32_t state) override;

  // Remaining PointerObserver methods are no-ops for CsdFrame.
  void notify_pointer_axis(Pointer*,
                           wl_pointer*,
                           uint32_t,
                           uint32_t,
                           double) override {}
  void notify_pointer_frame(Pointer*, wl_pointer*) override {}
  void notify_pointer_axis_source(Pointer*, wl_pointer*, uint32_t) override {}
  void notify_pointer_axis_stop(Pointer*,
                                wl_pointer*,
                                uint32_t,
                                uint32_t) override {}
  void notify_pointer_axis_discrete(Pointer*,
                                    wl_pointer*,
                                    uint32_t,
                                    int32_t) override {}

 private:
  explicit CsdFrame(XdgTopLevel* toplevel,
                    WindowManager* wm,
                    std::unique_ptr<CsdPlugin> plugin);

  XdgTopLevel* toplevel_;  // non-owning; owned by XdgWindowManager
  WindowManager* wm_;      // non-owning; owned by caller
  std::unique_ptr<CsdPlugin> plugin_;

  CsdFrameExtents extents_{};
  std::string title_;
  bool active_{false};
  bool ssd_active_{false};  // compositor mandated SERVER_SIDE
  bool visible_{true};

  // Last pointer enter serial and the surface it entered, for cursor updates
  // and interactive move/resize initiation.
  uint32_t pointer_serial_{};
  wl_surface* pointer_surface_{};

#if ENABLE_CSD
  // Double-click detection for title-bar maximize/restore.
  // Wayland button event time is in milliseconds (uint32_t, wraps ~49 days).
  static constexpr uint32_t kDblClickThresholdMs = 300u;
#endif

  // -------------------------------------------------------------------------
  // xdg-decoration protocol (per-window negotiation)
  // -------------------------------------------------------------------------
#if HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1
  zxdg_toplevel_decoration_v1* decoration_{};

  static void handle_decoration_configure(
      void* data,
      zxdg_toplevel_decoration_v1* decoration,
      uint32_t mode);

  static constexpr zxdg_toplevel_decoration_v1_listener decoration_listener_ = {
      .configure = handle_decoration_configure,
  };
#endif

  // Updates the pointer cursor to reflect the current hit zone.
  void apply_cursor(Pointer* pointer, CsdHitZone zone) const;
};
