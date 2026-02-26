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

#include <memory>
#include <string>

#include <wayland-client.h>

#include "waypp/window/buffer.h"
#include "waypp/window/csd_frame_extents.h"
#include "waypp/window/csd_hit_zone.h"
#include "waypp/window/csd_plugin.h"

/// Built-in CSD decoration renderer using Wayland SHM sub-surfaces.
///
/// Geometry (default extents: left=4, right=4, top=30, bottom=8):
///
///   ┌──────────────── top panel (30 px) ─────────────────┐
///   │  [─] [□] [×]   Title text                          │
///   ├──┬─────────────────────────────────────────────────┬──┤
///   │L │          content area                           │R │
///   │4 │                                                 │4 │
///   ├──┴─────────────────────────────────────────────────┴──┤
///   │               bottom panel (8 px)                     │
///   └────────────────────────────────────────────────────────┘
///
/// Colours (ARGB8888):
///   Active title bar   0xFF5294E2   Inactive   0xFF3A3A3A
///   Side/bottom border 0xFF222222
///   Close button       0xFFCC3333   Maximize   0xFF4A9A4A   Minimize
///   0xFF888888 Title text (white) 0xFFFFFFFF
///
/// No external rendering library is required — all drawing is pure pixel
/// fills into the SHM mapping.  An optional baked 5×7 ASCII glyph table
/// renders the title string without font dependencies.
class CsdShmPlugin final : public CsdPlugin {
 public:
  CsdShmPlugin() = default;

  bool init(wl_compositor* compositor,
            wl_subcompositor* subcompositor,
            wl_shm* shm,
            wl_surface* parent_surface) override;

  void update(const std::string& title, bool active) override;

  CsdFrameExtents resize(int32_t content_w, int32_t content_h) override;

  void commit() override;

  [[nodiscard]] CsdHitZone hit_test(wl_surface* surface,
                                    double x,
                                    double y) const override;

  void set_visible(bool visible) override;

  void destroy() override;

 private:
  // -------------------------------------------------------------------------
  // Panel — one subsurface strip + its SHM buffer
  // -------------------------------------------------------------------------
  struct Panel {
    wl_surface* surface{};
    wl_subsurface* subsurface{};
    std::unique_ptr<Buffer> buffer;
    int32_t x{}, y{}, w{}, h{};

    /// True when this panel has been successfully set up.
    [[nodiscard]] bool valid() const { return surface && subsurface; }
  };

  Panel top_;
  Panel left_;
  Panel right_;
  Panel bottom_;

  wl_compositor* compositor_{};
  wl_subcompositor* subcompositor_{};
  wl_shm* shm_{};
  wl_surface* parent_surface_{};

  std::string title_;
  bool active_{false};
  int32_t content_w_{};
  int32_t content_h_{};
  bool visible_{true};
  bool initialised_{false};

  // -------------------------------------------------------------------------
  // Default border sizes (logical pixels)
  // -------------------------------------------------------------------------
  static constexpr int32_t kTopH = 30;
  static constexpr int32_t kSideW = 4;
  static constexpr int32_t kBottomH = 8;

  // Button geometry within the top panel (right-aligned, 12×12, 6 px margin)
  static constexpr int32_t kBtnSize = 12;
  static constexpr int32_t kBtnMargin = 6;

  // Resize hit margin along panel edges (logical pixels)
  static constexpr int32_t kResizeEdge = 4;

  // -------------------------------------------------------------------------
  // Colours (ARGB8888)
  // -------------------------------------------------------------------------
  static constexpr uint32_t kColActiveTitleBar = 0xFF5294E2u;
  static constexpr uint32_t kColInactiveTitleBar = 0xFF3A3A3Au;
  static constexpr uint32_t kColBorder = 0xFF222222u;
  static constexpr uint32_t kColClose = 0xFFCC3333u;
  static constexpr uint32_t kColMaximize = 0xFF4A9A4Au;
  static constexpr uint32_t kColMinimize = 0xFF888888u;
  static constexpr uint32_t kColText = 0xFFFFFFFFu;

  // -------------------------------------------------------------------------
  // Helpers
  // -------------------------------------------------------------------------

  /// Create one wl_subsurface panel.
  bool create_panel(Panel& p, wl_surface* parent) const;

  /// Free all Wayland objects for one panel.
  static void destroy_panel(Panel& p);

  /// (Re)allocate and paint a panel's SHM buffer at (w × h), filled with
  /// @p fill_argb.  Returns false on allocation failure.
  bool paint_panel(Panel& p, int32_t w, int32_t h, uint32_t fill_argb);

  /// Overwrite a rectangle within @p p's pixel data with @p argb.
  static void fill_rect(const Panel& p,
                        int32_t rx,
                        int32_t ry,
                        int32_t rw,
                        int32_t rh,
                        uint32_t argb);

  /// Render the title string into the top panel using the baked glyph table.
  void paint_title(const std::string& text) const;

  /// Recompute and apply wl_subsurface_set_position for all four panels.
  void position_panels() const;

  /// Repaint the top panel (background + buttons + title).
  void repaint_top();

  // -------------------------------------------------------------------------
  // Baked 5×7 ASCII bitmap font (printable ASCII 0x20 - 0x7E)
  // -------------------------------------------------------------------------
  // Each glyph is 5 columns × 7 rows, stored MSB-first in 5 bytes.
  // Bit 4 (0x10) of each byte = leftmost column pixel.
  static constexpr int kGlyphW = 5;
  static constexpr int kGlyphH = 7;
  static const uint8_t kGlyphs[95][kGlyphH];
};
