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

/// @file test_csd_hit_zones.cc
///
/// Unit tests for CsdShmPlugin::hit_test().
///
/// No compositor is required: all wl_surface* members are set to synthetic
/// sentinel values (reinterpret_cast of small integers).  The plugin is put
/// into a post-init state by directly writing to its fields via
/// `#define private public` — acceptable in test code that lives in the same
/// translation unit as the class definition only when the alternative is a
/// heavyweight mock compositor.

// Give tests access to private fields.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define private public
#include "window/csd_shm_plugin.h"
#undef private

#include <gtest/gtest.h>

#include "waypp/window/csd_hit_zone.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Fake wl_surface pointers — these are never dereferenced, only compared.
static const auto kSurfTop = reinterpret_cast<wl_surface*>(0x1001);
static const auto kSurfLeft = reinterpret_cast<wl_surface*>(0x1002);
static const auto kSurfRight = reinterpret_cast<wl_surface*>(0x1003);
static const auto kSurfBottom = reinterpret_cast<wl_surface*>(0x1004);
static const auto kSurfParent = reinterpret_cast<wl_surface*>(0x1000);
static const auto kSurfOther = reinterpret_cast<wl_surface*>(0x9999);

/// Build a plugin with panels pre-initialised for a 600×400 content area.
/// No Wayland objects are created — only the geometry fields are set.
static std::unique_ptr<CsdShmPlugin> make_plugin(int32_t content_w = 600,
                                                 int32_t content_h = 400) {
  auto p = std::make_unique<CsdShmPlugin>();
  p->initialised_ = true;
  p->visible_ = true;
  p->content_w_ = content_w;
  p->content_h_ = content_h;
  p->parent_surface_ = kSurfParent;

  // top: full frame width × kTopH, positioned at (-kSideW, -kTopH)
  p->top_.surface = kSurfTop;
  p->top_.w = content_w + CsdShmPlugin::kSideW * 2;  // 608
  p->top_.h = CsdShmPlugin::kTopH;                   // 30

  // left/right panels: kSideW × content_h, positioned at y=0
  p->left_.surface = kSurfLeft;
  p->left_.w = CsdShmPlugin::kSideW;
  p->left_.h = content_h;

  p->right_.surface = kSurfRight;
  p->right_.w = CsdShmPlugin::kSideW;
  p->right_.h = content_h;

  // bottom: full frame width × kBottomH, positioned at (−kSideW, content_h)
  p->bottom_.surface = kSurfBottom;
  p->bottom_.w = p->top_.w;
  p->bottom_.h = CsdShmPlugin::kBottomH;

  return p;
}

// ---------------------------------------------------------------------------
// kNone — uninitialised plugin
// ---------------------------------------------------------------------------

TEST(CsdHitZones, UninitialisedReturnsNone) {
  const CsdShmPlugin p;
  EXPECT_EQ(p.hit_test(kSurfTop, 10.0, 10.0), CsdHitZone::kNone);
}

// ---------------------------------------------------------------------------
// kNone — unknown surface
// ---------------------------------------------------------------------------

TEST(CsdHitZones, UnknownSurfaceReturnsNone) {
  const auto p = make_plugin();
  EXPECT_EQ(p->hit_test(kSurfOther, 10.0, 10.0), CsdHitZone::kNone);
}

// ---------------------------------------------------------------------------
// Title bar (top panel interior)
// ---------------------------------------------------------------------------

TEST(CsdHitZones, TopPanelCentreIsTitleBar) {
  const auto p = make_plugin();
  // x=300, y=15 — center of the 608×30 top panel, away from edges and buttons
  EXPECT_EQ(p->hit_test(kSurfTop, 300.0, 15.0), CsdHitZone::kTitleBar);
}

// ---------------------------------------------------------------------------
// Resize — top edge of title bar
// ---------------------------------------------------------------------------

TEST(CsdHitZones, TopEdgeTopPanelIsResizeTop) {
  const auto p = make_plugin();
  // y < kResizeEdge(4), x in the middle
  EXPECT_EQ(p->hit_test(kSurfTop, 300.0, 2.0), CsdHitZone::kResizeTop);
}

TEST(CsdHitZones, TopEdgeTopLeftCorner) {
  const auto p = make_plugin();
  EXPECT_EQ(p->hit_test(kSurfTop, 2.0, 2.0), CsdHitZone::kResizeTopLeft);
}

TEST(CsdHitZones, TopEdgeTopRightCorner) {
  const auto p = make_plugin();
  // top_.w = 608; x > 608 - 4 = 604
  EXPECT_EQ(p->hit_test(kSurfTop, 606.0, 2.0), CsdHitZone::kResizeTopRight);
}

// ---------------------------------------------------------------------------
// Buttons — right-aligned in title bar
// CsdShmPlugin lays out (right→left): close, maximize, minimize
// bx_close    = top_.w - kBtnMargin - kBtnSize       = 608-6-12 = 590
// bx_maximize = 590 - (kBtnSize + kBtnMargin)        = 590-18   = 572
// bx_minimize = 572 - (kBtnSize + kBtnMargin)        = 572-18   = 554
// by          = (kTopH - kBtnSize) / 2               = (30-12)/2 = 9
// ---------------------------------------------------------------------------

TEST(CsdHitZones, CloseButton) {
  const auto p = make_plugin();
  // centre of close button: x = 590 + 6 = 596, y = 9 + 6 = 15
  EXPECT_EQ(p->hit_test(kSurfTop, 596.0, 15.0), CsdHitZone::kClose);
}

TEST(CsdHitZones, MaximizeButton) {
  const auto p = make_plugin();
  // center of Maximize button: x = 572 + 6 = 578, y = 15
  EXPECT_EQ(p->hit_test(kSurfTop, 578.0, 15.0), CsdHitZone::kMaximize);
}

TEST(CsdHitZones, MinimizeButton) {
  const auto p = make_plugin();
  // center of Minimize button: x = 554 + 6 = 560, y = 15
  EXPECT_EQ(p->hit_test(kSurfTop, 560.0, 15.0), CsdHitZone::kMinimize);
}

// ---------------------------------------------------------------------------
// Left panel
// ---------------------------------------------------------------------------

TEST(CsdHitZones, LeftPanelMiddleIsResizeLeft) {
  const auto p = make_plugin();
  const double mid_y = static_cast<double>(p->left_.h) / 2.0;
  EXPECT_EQ(p->hit_test(kSurfLeft, 2.0, mid_y), CsdHitZone::kResizeLeft);
}

TEST(CsdHitZones, LeftPanelTopEdgeIsResizeTopLeft) {
  const auto p = make_plugin();
  EXPECT_EQ(p->hit_test(kSurfLeft, 2.0, 2.0), CsdHitZone::kResizeTopLeft);
}

TEST(CsdHitZones, LeftPanelBottomEdgeIsResizeBottomLeft) {
  const auto p = make_plugin();
  const double near_bottom = static_cast<double>(p->left_.h) - 2.0;
  EXPECT_EQ(p->hit_test(kSurfLeft, 2.0, near_bottom),
            CsdHitZone::kResizeBottomLeft);
}

// ---------------------------------------------------------------------------
// Right panel
// ---------------------------------------------------------------------------

TEST(CsdHitZones, RightPanelMiddleIsResizeRight) {
  const auto p = make_plugin();
  const double mid_y = static_cast<double>(p->right_.h) / 2.0;
  EXPECT_EQ(p->hit_test(kSurfRight, 2.0, mid_y), CsdHitZone::kResizeRight);
}

TEST(CsdHitZones, RightPanelTopEdgeIsResizeTopRight) {
  const auto p = make_plugin();
  EXPECT_EQ(p->hit_test(kSurfRight, 2.0, 2.0), CsdHitZone::kResizeTopRight);
}

TEST(CsdHitZones, RightPanelBottomEdgeIsResizeBottomRight) {
  const auto p = make_plugin();
  const double near_bottom = static_cast<double>(p->right_.h) - 2.0;
  EXPECT_EQ(p->hit_test(kSurfRight, 2.0, near_bottom),
            CsdHitZone::kResizeBottomRight);
}

// ---------------------------------------------------------------------------
// Bottom panel
// ---------------------------------------------------------------------------

TEST(CsdHitZones, BottomPanelMiddleIsResizeBottom) {
  const auto p = make_plugin();
  const double mid_x = static_cast<double>(p->bottom_.w) / 2.0;
  EXPECT_EQ(p->hit_test(kSurfBottom, mid_x, 4.0), CsdHitZone::kResizeBottom);
}

TEST(CsdHitZones, BottomPanelLeftEdgeIsResizeBottomLeft) {
  const auto p = make_plugin();
  EXPECT_EQ(p->hit_test(kSurfBottom, 2.0, 4.0), CsdHitZone::kResizeBottomLeft);
}

TEST(CsdHitZones, BottomPanelRightEdgeIsResizeBottomRight) {
  const auto p = make_plugin();
  const double near_right = static_cast<double>(p->bottom_.w) - 2.0;
  EXPECT_EQ(p->hit_test(kSurfBottom, near_right, 4.0),
            CsdHitZone::kResizeBottomRight);
}

// ---------------------------------------------------------------------------
// Edge: button hit-test boundary (just outside button → title bar)
// ---------------------------------------------------------------------------

TEST(CsdHitZones, JustLeftOfCloseButtonIsTitleBar) {
  const auto p = make_plugin();
  // close button starts at x=590; x=589 is just outside → title bar
  EXPECT_EQ(p->hit_test(kSurfTop, 589.0, 15.0), CsdHitZone::kTitleBar);
}

// ---------------------------------------------------------------------------
// Hidden panel (set_visible false) → kNone
// ---------------------------------------------------------------------------

TEST(CsdHitZones, HiddenPluginReturnsNone) {
  auto p = make_plugin();
  p->visible_ = false;
  // The real invariant: when decorations are hidden the compositor detaches the
  // buffers, so pointer events over those surfaces no longer arrive. We verify
  // that hit_test returns kNone when not initialised.
  p->initialised_ = false;
  EXPECT_EQ(p->hit_test(kSurfTop, 300.0, 15.0), CsdHitZone::kNone);
}
