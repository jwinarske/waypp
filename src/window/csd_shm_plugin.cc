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

#include "csd_shm_plugin.h"

#include <algorithm>
#include <cstring>

#include "logging/logging.h"

// ---------------------------------------------------------------------------
// Baked 5×7 ASCII glyph table — printable ASCII 0x20 (' ') … 0x7E ('~')
// Each row byte: bits 4..0 = columns left→right (bit 4 = leftmost pixel).
// ---------------------------------------------------------------------------
// clang-format off
const uint8_t CsdShmPlugin::kGlyphs[95][CsdShmPlugin::kGlyphH] = {
  /* ' ' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  /* '!' */ {0x04,0x04,0x04,0x04,0x00,0x00,0x04},
  /* '"' */ {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00},
  /* '#' */ {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00},
  /* '$' */ {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},
  /* '%' */ {0x18,0x19,0x02,0x04,0x08,0x13,0x03},
  /* '&' */ {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D},
  /* ''' */ {0x04,0x04,0x00,0x00,0x00,0x00,0x00},
  /* '(' */ {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
  /* ')' */ {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
  /* '*' */ {0x00,0x04,0x15,0x0E,0x15,0x04,0x00},
  /* '+' */ {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
  /* ',' */ {0x00,0x00,0x00,0x00,0x06,0x04,0x08},
  /* '-' */ {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
  /* '.' */ {0x00,0x00,0x00,0x00,0x00,0x06,0x06},
  /* '/' */ {0x01,0x02,0x02,0x04,0x08,0x08,0x10},
  /* '0' */ {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
  /* '1' */ {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
  /* '2' */ {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
  /* '3' */ {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
  /* '4' */ {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
  /* '5' */ {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
  /* '6' */ {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
  /* '7' */ {0x1F,0x01,0x02,0x04,0x04,0x04,0x04},
  /* '8' */ {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
  /* '9' */ {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
  /* ':' */ {0x00,0x06,0x06,0x00,0x06,0x06,0x00},
  /* ';' */ {0x00,0x06,0x06,0x00,0x06,0x04,0x08},
  /* '<' */ {0x02,0x04,0x08,0x10,0x08,0x04,0x02},
  /* '=' */ {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},
  /* '>' */ {0x08,0x04,0x02,0x01,0x02,0x04,0x08},
  /* '?' */ {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},
  /* '@' */ {0x0E,0x11,0x01,0x0D,0x15,0x15,0x0E},
  /* 'A' */ {0x04,0x0A,0x11,0x11,0x1F,0x11,0x11},
  /* 'B' */ {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
  /* 'C' */ {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
  /* 'D' */ {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},
  /* 'E' */ {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
  /* 'F' */ {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
  /* 'G' */ {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
  /* 'H' */ {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
  /* 'I' */ {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
  /* 'J' */ {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
  /* 'K' */ {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
  /* 'L' */ {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
  /* 'M' */ {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
  /* 'N' */ {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
  /* 'O' */ {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
  /* 'P' */ {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
  /* 'Q' */ {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
  /* 'R' */ {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
  /* 'S' */ {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E},
  /* 'T' */ {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
  /* 'U' */ {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
  /* 'V' */ {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
  /* 'W' */ {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
  /* 'X' */ {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
  /* 'Y' */ {0x11,0x11,0x11,0x0A,0x04,0x04,0x04},
  /* 'Z' */ {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
  /* '[' */ {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E},
  /* '\\'*/ {0x10,0x08,0x08,0x04,0x02,0x02,0x01},
  /* ']' */ {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E},
  /* '^' */ {0x04,0x0A,0x11,0x00,0x00,0x00,0x00},
  /* '_' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x1F},
  /* '`' */ {0x08,0x04,0x00,0x00,0x00,0x00,0x00},
  /* 'a' */ {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F},
  /* 'b' */ {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E},
  /* 'c' */ {0x00,0x00,0x0E,0x10,0x10,0x11,0x0E},
  /* 'd' */ {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F},
  /* 'e' */ {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E},
  /* 'f' */ {0x06,0x09,0x08,0x1C,0x08,0x08,0x08},
  /* 'g' */ {0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E},
  /* 'h' */ {0x10,0x10,0x1E,0x11,0x11,0x11,0x11},
  /* 'i' */ {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E},
  /* 'j' */ {0x02,0x00,0x06,0x02,0x02,0x12,0x0C},
  /* 'k' */ {0x10,0x10,0x11,0x12,0x1C,0x12,0x11},
  /* 'l' */ {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E},
  /* 'm' */ {0x00,0x00,0x1A,0x15,0x15,0x11,0x11},
  /* 'n' */ {0x00,0x00,0x1E,0x11,0x11,0x11,0x11},
  /* 'o' */ {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E},
  /* 'p' */ {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10},
  /* 'q' */ {0x00,0x00,0x0F,0x11,0x0F,0x01,0x01},
  /* 'r' */ {0x00,0x00,0x16,0x19,0x10,0x10,0x10},
  /* 's' */ {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E},
  /* 't' */ {0x08,0x08,0x1C,0x08,0x08,0x09,0x06},
  /* 'u' */ {0x00,0x00,0x11,0x11,0x11,0x13,0x0D},
  /* 'v' */ {0x00,0x00,0x11,0x11,0x11,0x0A,0x04},
  /* 'w' */ {0x00,0x00,0x11,0x11,0x15,0x15,0x0A},
  /* 'x' */ {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11},
  /* 'y' */ {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E},
  /* 'z' */ {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F},
  /* '{' */ {0x02,0x04,0x04,0x08,0x04,0x04,0x02},
  /* '|' */ {0x04,0x04,0x04,0x04,0x04,0x04,0x04},
  /* '}' */ {0x08,0x04,0x04,0x02,0x04,0x04,0x08},
  /* '~' */ {0x00,0x08,0x15,0x02,0x00,0x00,0x00},
};
// clang-format on

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool CsdShmPlugin::create_panel(Panel& p, wl_surface* parent) const {
  p.surface = wl_compositor_create_surface(compositor_);
  if (!p.surface) {
    LOG_ERROR("[CsdShmPlugin] wl_compositor_create_surface failed");
    return false;
  }
  p.subsurface =
      wl_subcompositor_get_subsurface(subcompositor_, p.surface, parent);
  if (!p.subsurface) {
    LOG_ERROR("[CsdShmPlugin] wl_subcompositor_get_subsurface failed");
    wl_surface_destroy(p.surface);
    p.surface = nullptr;
    return false;
  }
  // Decorations are drawn synchronously with the parent.
  wl_subsurface_set_sync(p.subsurface);
  return true;
}

void CsdShmPlugin::destroy_panel(Panel& p) {
  p.buffer.reset();
  if (p.subsurface) {
    wl_subsurface_destroy(p.subsurface);
    p.subsurface = nullptr;
  }
  if (p.surface) {
    wl_surface_destroy(p.surface);
    p.surface = nullptr;
  }
  p.x = p.y = p.w = p.h = 0;
}

bool CsdShmPlugin::paint_panel(Panel& p,
                               const int32_t w,
                               const int32_t h,
                               const uint32_t fill_argb) {
  if (w <= 0 || h <= 0) {
    return true;  // nothing to do
  }

  // Recreate the buffer whenever dimensions change.
  if (!p.buffer || p.buffer->get_width() != w || p.buffer->get_height() != h) {
    if (p.buffer) {
      p.buffer->destroy();
    } else {
      p.buffer = std::make_unique<Buffer>(shm_);
    }
    if (p.buffer->create_shm_buffer(w, h, WL_SHM_FORMAT_ARGB8888) < 0) {
      LOG_ERROR("[CsdShmPlugin] create_shm_buffer({}×{}) failed", w, h);
      p.buffer.reset();
      return false;
    }
  }

  p.w = w;
  p.h = h;

  // Flood-fill the entire panel.
  const int32_t num_pixels = w * h;
  auto* px = static_cast<uint32_t*>(p.buffer->get_shm_data());
  std::fill_n(px, num_pixels, fill_argb);

  return true;
}

void CsdShmPlugin::fill_rect(const Panel& p,
                             const int32_t rx,
                             const int32_t ry,
                             const int32_t rw,
                             const int32_t rh,
                             const uint32_t argb) {
  if (!p.buffer || rw <= 0 || rh <= 0)
    return;
  auto* px = static_cast<uint32_t*>(p.buffer->get_shm_data());
  const int32_t stride = p.w;
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic) --
  // Pixel buffer access; bounds are enforced by the loop guards above.
  for (int32_t y = ry; y < ry + rh && y < p.h; ++y) {
    for (int32_t x = rx; x < rx + rw && x < p.w; ++x) {
      px[y * stride + x] = argb;
    }
  }
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

void CsdShmPlugin::paint_title(const std::string& text) const {
  if (!top_.buffer || text.empty())
    return;

  auto* px = static_cast<uint32_t*>(top_.buffer->get_shm_data());
  const int32_t stride = top_.w;

  // Start 8 px from the left edge, vertically centred in the top panel.
  // Reserve space for three right-aligned buttons.
  constexpr int32_t btn_area = 3 * (kBtnSize + kBtnMargin) + kBtnMargin;
  const int32_t max_x = top_.w - btn_area - kBtnMargin;
  int32_t cx = 8;
  const int32_t cy = (top_.h - kGlyphH) / 2;

  for (const char raw : text) {
    const auto ch = static_cast<unsigned char>(raw);
    if (cx + kGlyphW >= max_x)
      break;
    if (ch < 0x20 || ch > 0x7E) {
      cx += kGlyphW + 1;
      continue;
    }
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay,cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    // ch is validated in [0x20,0x7E]; kGlyphs[ch-0x20] is in-bounds.
    // glyph[gy] and px[...] use pointer arithmetic over fixed-size buffers
    // whose bounds are enforced by the loop guards.
    const auto* glyph = kGlyphs[ch - 0x20];
    for (int gy = 0; gy < kGlyphH; ++gy) {
      const int32_t py = cy + gy;
      if (py < 0 || py >= top_.h)
        continue;
      for (int gx = 0; gx < kGlyphW; ++gx) {
        if (glyph[gy] & (0x10u >> gx)) {
          if (const int32_t px_x = cx + gx; px_x >= 0 && px_x < top_.w) {
            px[py * stride + px_x] = kColText;
          }
        }
      }
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay,cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    cx += kGlyphW + 1;
  }
}

void CsdShmPlugin::position_panels() const {
  if (top_.valid()) {
    // The top panel spans the full frame width (content_w + kSideW*2) and must
    // be offset left by kSideW, so it aligns with the outer edge of the side
    // panels.
    wl_subsurface_set_position(top_.subsurface, -kSideW, -kTopH);
  }
  if (left_.valid()) {
    wl_subsurface_set_position(left_.subsurface, -kSideW, 0);
  }
  if (right_.valid()) {
    wl_subsurface_set_position(right_.subsurface, content_w_, 0);
  }
  if (bottom_.valid()) {
    wl_subsurface_set_position(bottom_.subsurface, -kSideW, content_h_);
  }
}

void CsdShmPlugin::repaint_top() {
  if (!top_.valid())
    return;
  if (top_.w <= 0 || top_.h <= 0)
    return;

  if (const uint32_t bg = active_ ? kColActiveTitleBar : kColInactiveTitleBar;
      !paint_panel(top_, top_.w, top_.h, bg))
    return;

  // Draw three buttons right-aligned: [minimize] [maximize] [close]
  // right edge → close → maximize → minimize
  int32_t bx = top_.w - kBtnMargin - kBtnSize;
  const int32_t by = (top_.h - kBtnSize) / 2;

  fill_rect(top_, bx, by, kBtnSize, kBtnSize, kColClose);
  bx -= (kBtnSize + kBtnMargin);
  fill_rect(top_, bx, by, kBtnSize, kBtnSize, kColMaximize);
  bx -= (kBtnSize + kBtnMargin);
  fill_rect(top_, bx, by, kBtnSize, kBtnSize, kColMinimize);

  paint_title(title_);

  // Mark the whole panel dirty.
  wl_surface_damage(top_.surface, 0, 0, top_.w, top_.h);
  wl_surface_attach(top_.surface, top_.buffer->get_wl_buffer(), 0, 0);
}

// ---------------------------------------------------------------------------
// CsdPlugin interface
// ---------------------------------------------------------------------------

bool CsdShmPlugin::init(wl_compositor* compositor,
                        wl_subcompositor* subcompositor,
                        wl_shm* shm,
                        wl_surface* parent_surface) {
  DLOG_TRACE("++CsdShmPlugin::init()");

  compositor_ = compositor;
  subcompositor_ = subcompositor;
  shm_ = shm;
  parent_surface_ = parent_surface;

  if (!create_panel(top_, parent_surface) ||
      !create_panel(left_, parent_surface) ||
      !create_panel(right_, parent_surface) ||
      !create_panel(bottom_, parent_surface)) {
    destroy();
    return false;
  }

  initialised_ = true;
  DLOG_TRACE("--CsdShmPlugin::init()");
  return true;
}

void CsdShmPlugin::update(const std::string& title, const bool active) {
  title_ = title;
  active_ = active;
  repaint_top();
}

CsdFrameExtents CsdShmPlugin::resize(const int32_t content_w,
                                     const int32_t content_h) {
  if (!initialised_ || !visible_) {
    return {};
  }

  content_w_ = content_w;
  content_h_ = content_h;

  const int32_t frame_w = content_w + kSideW * 2;

  // The top panel spans full frame width; positioned at (-kSideW, -kTopH).
  top_.w = frame_w;
  top_.h = kTopH;
  repaint_top();

  // Left/right panels cover only the content height (y=0..content_h).
  // The top panel covers the title bar rows; the bottom panel covers the
  // footer.
  paint_panel(left_, kSideW, content_h, kColBorder);
  wl_surface_damage(left_.surface, 0, 0, left_.w, left_.h);
  wl_surface_attach(left_.surface,
                    left_.buffer ? left_.buffer->get_wl_buffer() : nullptr, 0,
                    0);

  paint_panel(right_, kSideW, content_h, kColBorder);
  wl_surface_damage(right_.surface, 0, 0, right_.w, right_.h);
  wl_surface_attach(right_.surface,
                    right_.buffer ? right_.buffer->get_wl_buffer() : nullptr, 0,
                    0);

  // The bottom panel spans full frame width; positioned at (-kSideW,
  // content_h).
  paint_panel(bottom_, frame_w, kBottomH, kColBorder);
  wl_surface_damage(bottom_.surface, 0, 0, bottom_.w, bottom_.h);
  wl_surface_attach(bottom_.surface,
                    bottom_.buffer ? bottom_.buffer->get_wl_buffer() : nullptr,
                    0, 0);

  position_panels();

  return {kSideW, kSideW, kTopH, kBottomH};
}

void CsdShmPlugin::commit() {
  if (!initialised_)
    return;

  if (top_.valid())
    wl_surface_commit(top_.surface);
  if (left_.valid())
    wl_surface_commit(left_.surface);
  if (right_.valid())
    wl_surface_commit(right_.surface);
  if (bottom_.valid())
    wl_surface_commit(bottom_.surface);
}

CsdHitZone CsdShmPlugin::hit_test(wl_surface* const surface,
                                  const double x,
                                  const double y) const {
  if (!initialised_)
    return CsdHitZone::kNone;

  // -----------------------------------------------------------------------
  // Top panel
  // -----------------------------------------------------------------------
  if (surface == top_.surface || surface == parent_surface_) {
    // Pointer coordinates are relative to the surface they entered, but when
    // entering the parent surface near the top edge, the y offset may be < 0
    // when the panel is positioned at -kTopH.  Treat the top panel as the
    // active zone when surface == top_.surface.

    if (surface == top_.surface) {
      const auto ix = static_cast<int32_t>(x);
      const auto iy = static_cast<int32_t>(y);

      // Resize edge along the very top of the title bar.
      if (iy < kResizeEdge) {
        if (ix < kResizeEdge)
          return CsdHitZone::kResizeTopLeft;
        if (ix > top_.w - kResizeEdge)
          return CsdHitZone::kResizeTopRight;
        return CsdHitZone::kResizeTop;
      }

      // Button hit-test (right-aligned, bottom half of panel).
      if (const int32_t by = (top_.h - kBtnSize) / 2;
          iy >= by && iy < by + kBtnSize) {
        // close
        int32_t bx = top_.w - kBtnMargin - kBtnSize;
        if (ix >= bx && ix < bx + kBtnSize)
          return CsdHitZone::kClose;
        // maximize
        bx -= (kBtnSize + kBtnMargin);
        if (ix >= bx && ix < bx + kBtnSize)
          return CsdHitZone::kMaximize;
        // minimize
        bx -= (kBtnSize + kBtnMargin);
        if (ix >= bx && ix < bx + kBtnSize)
          return CsdHitZone::kMinimize;
      }

      return CsdHitZone::kTitleBar;
    }
  }

  // -----------------------------------------------------------------------
  // Side panels — resize only
  // -----------------------------------------------------------------------
  if (surface == left_.surface) {
    const auto iy = static_cast<int32_t>(y);
    if (iy < kResizeEdge)
      return CsdHitZone::kResizeTopLeft;
    if (iy > left_.h - kResizeEdge)
      return CsdHitZone::kResizeBottomLeft;
    return CsdHitZone::kResizeLeft;
  }

  if (surface == right_.surface) {
    const auto iy = static_cast<int32_t>(y);
    if (iy < kResizeEdge)
      return CsdHitZone::kResizeTopRight;
    if (iy > right_.h - kResizeEdge)
      return CsdHitZone::kResizeBottomRight;
    return CsdHitZone::kResizeRight;
  }

  // -----------------------------------------------------------------------
  // Bottom panel — resize only
  // -----------------------------------------------------------------------
  if (surface == bottom_.surface) {
    const auto ix = static_cast<int32_t>(x);
    if (ix < kResizeEdge)
      return CsdHitZone::kResizeBottomLeft;
    if (ix > bottom_.w - kResizeEdge)
      return CsdHitZone::kResizeBottomRight;
    return CsdHitZone::kResizeBottom;
  }

  return CsdHitZone::kNone;
}

void CsdShmPlugin::set_visible(const bool visible) {
  visible_ = visible;
  if (!initialised_)
    return;

  if (!visible) {
    // Detach all buffers → compositor hides the subsurfaces.
    auto detach = [](Panel& p) {
      if (p.valid()) {
        wl_surface_attach(p.surface, nullptr, 0, 0);
        wl_surface_commit(p.surface);
      }
      // Reset dimensions so paint_panel() unconditionally reallocates the
      // buffer on the next resize(), guaranteeing correct size after restore.
      p.w = 0;
      p.h = 0;
    };
    detach(top_);
    detach(left_);
    detach(right_);
    detach(bottom_);
  }
  // Re attachment happens on the next resize() + commit() cycle.
}

void CsdShmPlugin::destroy() {
  DLOG_TRACE("++CsdShmPlugin::destroy()");
  destroy_panel(top_);
  destroy_panel(left_);
  destroy_panel(right_);
  destroy_panel(bottom_);
  initialised_ = false;
  compositor_ = nullptr;
  subcompositor_ = nullptr;
  shm_ = nullptr;
  parent_surface_ = nullptr;
  DLOG_TRACE("--CsdShmPlugin::destroy()");
}
