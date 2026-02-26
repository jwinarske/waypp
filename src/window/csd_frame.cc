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

#include "waypp/window/csd_frame.h"

#include <linux/input.h>

#include "csd_shm_plugin.h"
#include "logging/logging.h"
#include "waypp/window/xdg_toplevel.h"
#include "waypp/window_manager/window_manager.h"

// ---------------------------------------------------------------------------
// Cursor name lookup table for CsdHitZone → XCursor name mapping.
// Used by apply_cursor() to set the right resize / default cursor via the
// existing Pointer::set_cursor() API (which falls back to wl_cursor_theme
// when wp_cursor_shape_manager_v1 is not available).
// ---------------------------------------------------------------------------
static constexpr const char* zone_cursor(const CsdHitZone zone) {
  switch (zone) {
    case CsdHitZone::kResizeTop:
      return "n-resize";
    case CsdHitZone::kResizeBottom:
      return "s-resize";
    case CsdHitZone::kResizeLeft:
      return "w-resize";
    case CsdHitZone::kResizeRight:
      return "e-resize";
    case CsdHitZone::kResizeTopLeft:
      return "nw-resize";
    case CsdHitZone::kResizeTopRight:
      return "ne-resize";
    case CsdHitZone::kResizeBottomLeft:
      return "sw-resize";
    case CsdHitZone::kResizeBottomRight:
      return "se-resize";
    case CsdHitZone::kTitleBar:
    case CsdHitZone::kClose:
    case CsdHitZone::kMaximize:
    case CsdHitZone::kMinimize:
    case CsdHitZone::kNone:
    default:
      return "left_ptr";
  }
}

// ---------------------------------------------------------------------------
// CsdFrame — private constructor
// ---------------------------------------------------------------------------

CsdFrame::CsdFrame(XdgTopLevel* toplevel,
                   WindowManager* wm,
                   std::unique_ptr<CsdPlugin> plugin)
    : toplevel_(toplevel), wm_(wm), plugin_(std::move(plugin)) {}

// ---------------------------------------------------------------------------
// CsdFrame::create — factory
// ---------------------------------------------------------------------------

std::unique_ptr<CsdFrame> CsdFrame::create(XdgTopLevel* toplevel,
                                           WindowManager* wm,
                                           const std::string& title,
                                           std::unique_ptr<CsdPlugin> plugin) {
  DLOG_TRACE("++CsdFrame::create()");

  if (!plugin) {
    plugin = std::make_unique<CsdShmPlugin>();
  }

  // Use private constructor — std::make_unique cannot access it.
  auto frame =
      std::unique_ptr<CsdFrame>(new CsdFrame(toplevel, wm, std::move(plugin)));

  frame->title_ = title;

#if HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1
  if (auto* manager = wm->get_xdg_decoration_manager()) {
    DLOG_DEBUG("[CsdFrame] Negotiating CSD via zxdg_decoration_manager_v1");
    frame->decoration_ = zxdg_decoration_manager_v1_get_toplevel_decoration(
        manager, toplevel->get_xdg_toplevel());
    if (frame->decoration_) {
      zxdg_toplevel_decoration_v1_add_listener(
          frame->decoration_, &decoration_listener_, frame.get());
      // Request client-side; compositor may override with SERVER_SIDE.
      zxdg_toplevel_decoration_v1_set_mode(
          frame->decoration_, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
      // The compositor will fire handle_decoration_configure before the next
      // configure event.  init() is called there if CLIENT_SIDE is confirmed.
      DLOG_TRACE("--CsdFrame::create() [decoration pending]");
      return frame;
    }
    LOG_WARN(
        "[CsdFrame] zxdg_decoration_manager_v1_get_toplevel_decoration failed");
  }
#endif

  // No decoration manager or binding failed — assume CSD is required.
  const bool ok =
      frame->plugin_->init(wm->get_compositor(), wm->get_subcompositor(),
                           wm->get_shm(), toplevel->get_surface());
  if (!ok) {
    LOG_ERROR(
        "[CsdFrame] CsdPlugin::init() failed — running without decorations");
    frame->plugin_.reset();
  }

  // Register as a PointerObserver so we receive pointer events for hit-testing.
  if (const auto seat = wm->get_seat();
      seat.has_value() && seat.value()->get_pointer().has_value()) {
    seat.value()->get_pointer().value()->register_observer(frame.get());
  }

  DLOG_TRACE("--CsdFrame::create()");
  return frame;
}

// ---------------------------------------------------------------------------
// CsdFrame::~CsdFrame
// ---------------------------------------------------------------------------

CsdFrame::~CsdFrame() {
  DLOG_TRACE("++CsdFrame::~CsdFrame()");

  // Unregister pointer observer.
  if (const auto seat = wm_->get_seat();
      seat.has_value() && seat.value()->get_pointer().has_value()) {
    seat.value()->get_pointer().value()->unregister_observer(this);
  }

  if (plugin_) {
    plugin_->destroy();
    plugin_.reset();
  }

#if HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1
  if (decoration_) {
    DLOG_TRACE("[CsdFrame] zxdg_toplevel_decoration_v1_destroy");
    zxdg_toplevel_decoration_v1_destroy(decoration_);
    decoration_ = nullptr;
  }
#endif

  DLOG_TRACE("--CsdFrame::~CsdFrame()");
}

// ---------------------------------------------------------------------------
// CsdFrame::on_configure
// ---------------------------------------------------------------------------

void CsdFrame::on_configure(const int32_t content_w,
                            const int32_t content_h,
                            const bool active,
                            const bool fullscreen,
                            const bool maximized,
                            const bool tiled) {
  if (!plugin_ || ssd_active_) {
    return;
  }

  const bool floating = !fullscreen && !maximized && !tiled;

  // Update visibility FIRST so that plugin_->resize() doesn't early-return
  // due to visible_==false (e.g. when restoring from maximized/fullscreen).
  if (floating != visible_) {
    set_visible(floating);
  }

  // Resize (and allocate/realloc buffers) after visibility is current.
  extents_ = plugin_->resize(content_w, content_h);

  if (!floating) {
    extents_ = {};
  }

  // update() repaints the title bar — safe now that resize() ran above.
  if (active != active_) {
    active_ = active;
  }
  plugin_->update(title_, active_);
}

// ---------------------------------------------------------------------------
// CsdFrame::commit
// ---------------------------------------------------------------------------

void CsdFrame::commit() const {
  if (plugin_ && !ssd_active_) {
    plugin_->commit();
  }
}

// ---------------------------------------------------------------------------
// CsdFrame::set_visible
// ---------------------------------------------------------------------------

void CsdFrame::set_visible(const bool visible) {
  visible_ = visible;
  if (plugin_ && !ssd_active_) {
    plugin_->set_visible(visible);
  }
}

// ---------------------------------------------------------------------------
// CsdFrame::set_title
// ---------------------------------------------------------------------------

void CsdFrame::set_title(const std::string& title) {
  title_ = title;
  if (plugin_ && !ssd_active_) {
    plugin_->update(title_, active_);
  }
}

// ---------------------------------------------------------------------------
// PointerObserver implementation
// ---------------------------------------------------------------------------

void CsdFrame::notify_pointer_enter(Pointer* pointer,
                                    wl_pointer* /* wl_pointer */,
                                    const uint32_t serial,
                                    wl_surface* surface,
                                    const double sx,
                                    const double sy) {
  pointer_serial_ = serial;
  pointer_surface_ = surface;

  if (!plugin_ || ssd_active_) {
    return;
  }

  const auto zone = plugin_->hit_test(surface, sx, sy);
  apply_cursor(pointer, zone);
}

void CsdFrame::notify_pointer_leave(Pointer* /* pointer */,
                                    wl_pointer* /* wl_pointer */,
                                    uint32_t /* serial */,
                                    wl_surface* /* surface */) {
  pointer_surface_ = nullptr;
}

void CsdFrame::notify_pointer_motion(Pointer* pointer,
                                     wl_pointer* /* wl_pointer */,
                                     uint32_t /* time */,
                                     const double sx,
                                     const double sy) {
  if (!plugin_ || ssd_active_ || !pointer_surface_) {
    return;
  }

  const auto zone = plugin_->hit_test(pointer_surface_, sx, sy);
  apply_cursor(pointer, zone);
}

void CsdFrame::notify_pointer_button(Pointer* pointer,
                                     wl_pointer* /* wl_pointer */,
                                     const uint32_t serial,
                                     uint32_t /* time */,
                                     const uint32_t button,
                                     const uint32_t state) {
  if (!plugin_ || ssd_active_ || !pointer_surface_) {
    return;
  }

  if (button != BTN_LEFT || state != WL_POINTER_BUTTON_STATE_PRESSED) {
    return;
  }

  const auto zone = plugin_->hit_test(pointer_surface_, pointer->get_xy().first,
                                      pointer->get_xy().second);

  const auto* seat = wm_->get_seat().value_or(nullptr);
  if (!seat) {
    return;
  }
  auto* wl_seat = seat->get_seat();

  switch (zone) {
    case CsdHitZone::kTitleBar:
      DLOG_DEBUG("[CsdFrame] interactive move");
      toplevel_->move(wl_seat, serial);
      break;

    case CsdHitZone::kClose:
      DLOG_DEBUG("[CsdFrame] close");
      toplevel_->close();
      break;

    case CsdHitZone::kMaximize:
      DLOG_DEBUG("[CsdFrame] maximize/restore");
      if (toplevel_->get_maximized()) {
        xdg_toplevel_unset_maximized(toplevel_->get_xdg_toplevel());
      } else {
        toplevel_->set_maximize();
      }
      break;

    case CsdHitZone::kMinimize:
      DLOG_DEBUG("[CsdFrame] minimize");
      toplevel_->set_minimize();
      break;

    case CsdHitZone::kResizeTop:
    case CsdHitZone::kResizeBottom:
    case CsdHitZone::kResizeLeft:
    case CsdHitZone::kResizeRight:
    case CsdHitZone::kResizeTopLeft:
    case CsdHitZone::kResizeTopRight:
    case CsdHitZone::kResizeBottomLeft:
    case CsdHitZone::kResizeBottomRight:
      DLOG_DEBUG("[CsdFrame] interactive resize edge={}",
                 static_cast<int>(zone));
      toplevel_->resize(wl_seat, serial, static_cast<uint32_t>(zone));
      break;

    case CsdHitZone::kNone:
    default:
      break;
  }

  (void)pointer;
  (void)pointer_serial_;
}

// ---------------------------------------------------------------------------
// apply_cursor — set pointer shape for the current hit zone
// ---------------------------------------------------------------------------

void CsdFrame::apply_cursor(Pointer* pointer, const CsdHitZone zone) const {
  pointer->set_cursor(pointer_serial_, zone_cursor(zone));
}

// ---------------------------------------------------------------------------
// xdg-decoration protocol callback
// ---------------------------------------------------------------------------

#if HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1
void CsdFrame::handle_decoration_configure(
    void* data,
    zxdg_toplevel_decoration_v1* /* decoration */,
    const uint32_t mode) {
  auto* self = static_cast<CsdFrame*>(data);

  if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE) {
    LOG_DEBUG("[CsdFrame] Compositor chose SERVER_SIDE decorations");
    self->ssd_active_ = true;
    self->extents_ = {};
    // Plugin was never initialized — nothing to tear down.
    return;
  }

  LOG_DEBUG("[CsdFrame] Compositor confirmed CLIENT_SIDE decorations");
  self->ssd_active_ = false;

  const bool ok = self->plugin_->init(
      self->wm_->get_compositor(), self->wm_->get_subcompositor(),
      self->wm_->get_shm(), self->toplevel_->get_surface());
  if (!ok) {
    LOG_ERROR("[CsdFrame] CsdPlugin::init() failed after decoration negotiate");
    self->plugin_.reset();
    return;
  }

  self->plugin_->update(self->title_, self->active_);

  // Register as PointerObserver now that CSD is confirmed.
  if (const auto seat = self->wm_->get_seat();
      seat.has_value() && seat.value()->get_pointer().has_value()) {
    seat.value()->get_pointer().value()->register_observer(self);
  }
}
#endif
