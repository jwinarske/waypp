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
/// into a post-init state by directly writing to its private fields through
/// the CsdShmPluginTest friend class declared in csd_shm_plugin.h.
/// This is ODR-safe: there is exactly one definition of CsdShmPlugin in the
/// link and no macro rewriting of the class layout.

#include "window/csd_shm_plugin.h"

#include <gtest/gtest.h>

#include "waypp/window/csd_hit_zone.h"

// ---------------------------------------------------------------------------
// Fake wl_surface pointers — never dereferenced, only compared as identities.
// SentinelSurface wraps the single unavoidable reinterpret_cast in one place.
// The globals are suppressed for avoid-non-const-global-variables: wl_surface
// is an opaque C API type whose pointed-to data cannot be const-qualified
// because the struct members they are assigned to are wl_surface* (non-const).
// ---------------------------------------------------------------------------
static wl_surface* SentinelSurface(const uintptr_t addr) {
  return static_cast<wl_surface*>(reinterpret_cast<void*>(
      addr));  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) -- the
               // only portable way to produce a non-null sentinel from an
               // integer in C++17
}
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables) -- wl_surface
// is a C opaque type; pointed-to data cannot be const-qualified
static wl_surface* const kSurfTop = SentinelSurface(0x1001);
static wl_surface* const kSurfLeft = SentinelSurface(0x1002);
static wl_surface* const kSurfRight = SentinelSurface(0x1003);
static wl_surface* const kSurfBottom = SentinelSurface(0x1004);
static wl_surface* const kSurfParent = SentinelSurface(0x1000);
static wl_surface* const kSurfOther = SentinelSurface(0x9999);
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// ---------------------------------------------------------------------------
// CsdShmPluginTest — Google Test fixture and test-seam friend class.
//
// CsdShmPlugin declares `friend class CsdShmPluginTest`; which grants this
// class access to all private members.  TEST_F tests run as methods of this
// class, so they also have that access.  This replaces the ODR-unsafe
// `#define private public` pattern with a standard, well-defined seam.
// ---------------------------------------------------------------------------
class CsdShmPluginTest : public ::testing::Test {
 protected:
  /// Build a plugin with panels pre-initialized for a content_w x content_h
  /// content area.  No Wayland objects are created; only geometry is set.
  static std::unique_ptr<CsdShmPlugin> make_plugin(int32_t content_w = 600,
                                                   int32_t content_h = 400) {
    auto p = std::make_unique<CsdShmPlugin>();
    p->initialised_ = true;
    p->visible_ = true;
    p->content_w_ = content_w;
    p->content_h_ = content_h;
    p->parent_surface_ = kSurfParent;

    // top: full frame width x kTopH
    p->top_.surface = kSurfTop;
    p->top_.w = content_w + CsdShmPlugin::kSideW * 2;  // 608
    p->top_.h = CsdShmPlugin::kTopH;                   // 30

    // left / right: kSideW x content_h
    p->left_.surface = kSurfLeft;
    p->left_.w = CsdShmPlugin::kSideW;
    p->left_.h = content_h;

    p->right_.surface = kSurfRight;
    p->right_.w = CsdShmPlugin::kSideW;
    p->right_.h = content_h;

    // bottom: full frame width x kBottomH
    p->bottom_.surface = kSurfBottom;
    p->bottom_.w = p->top_.w;
    p->bottom_.h = CsdShmPlugin::kBottomH;

    return p;
  }

  // Accessors for private panel dimensions used in individual test bodies.
  // TEST_F subclasses inherit from CsdShmPluginTest but C++ does not
  // propagate friend access through inheritance, so the compiler
  //  rejects direct 'p->left_.h' in a TEST_F body.  Routing through these
  // static methods keeps all private access inside the friend class.
  static int32_t left_h(const CsdShmPlugin& p) { return p.left_.h; }
  static int32_t right_h(const CsdShmPlugin& p) { return p.right_.h; }
  static int32_t bottom_w(const CsdShmPlugin& p) { return p.bottom_.w; }
  static void set_visible(CsdShmPlugin& p, bool v) { p.visible_ = v; }
  static void set_initialised(CsdShmPlugin& p, bool v) { p.initialised_ = v; }
};

// ---------------------------------------------------------------------------
// kNone -- uninitialised plugin
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginTest, UninitialisedReturnsNone) {
  const CsdShmPlugin p;
  EXPECT_EQ(p.hit_test(kSurfTop, 10.0, 10.0), CsdHitZone::kNone);
}

// ---------------------------------------------------------------------------
// kNone -- unknown surface
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginTest, UnknownSurfaceReturnsNone) {
  const auto p = make_plugin();
  EXPECT_EQ(p->hit_test(kSurfOther, 10.0, 10.0), CsdHitZone::kNone);
}

// ---------------------------------------------------------------------------
// Title bar (top panel interior)
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginTest, TopPanelCentreIsTitleBar) {
  const auto p = make_plugin();
  // x=300, y=15 -- centre of the 608x30 top panel, away from edges/buttons
  EXPECT_EQ(p->hit_test(kSurfTop, 300.0, 15.0), CsdHitZone::kTitleBar);
}

// ---------------------------------------------------------------------------
// Resize -- top edge of title bar
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginTest, TopEdgeTopPanelIsResizeTop) {
  const auto p = make_plugin();
  // y < kResizeEdge(4), x in the middle
  EXPECT_EQ(p->hit_test(kSurfTop, 300.0, 2.0), CsdHitZone::kResizeTop);
}

TEST_F(CsdShmPluginTest, TopEdgeTopLeftCorner) {
  const auto p = make_plugin();
  EXPECT_EQ(p->hit_test(kSurfTop, 2.0, 2.0), CsdHitZone::kResizeTopLeft);
}

TEST_F(CsdShmPluginTest, TopEdgeTopRightCorner) {
  const auto p = make_plugin();
  // top_.w = 608; x > 608 - 4 = 604
  EXPECT_EQ(p->hit_test(kSurfTop, 606.0, 2.0), CsdHitZone::kResizeTopRight);
}

// ---------------------------------------------------------------------------
// Buttons -- right-aligned in title bar
// CsdShmPlugin lays out (right->left): close, maximize, minimize
//   bx_close    = top_.w - kBtnMargin - kBtnSize       = 608-6-12 = 590
//   bx_maximize = 590 - (kBtnSize + kBtnMargin)        = 590-18   = 572
//   bx_minimize = 572 - (kBtnSize + kBtnMargin)        = 572-18   = 554
//   by          = (kTopH - kBtnSize) / 2               = (30-12)/2 = 9
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginTest, CloseButton) {
  const auto p = make_plugin();
  // center of close button: x = 590 + 6 = 596, y = 9 + 6 = 15
  EXPECT_EQ(p->hit_test(kSurfTop, 596.0, 15.0), CsdHitZone::kClose);
}

TEST_F(CsdShmPluginTest, MaximizeButton) {
  const auto p = make_plugin();
  // center of Maximize button: x = 572 + 6 = 578, y = 15
  EXPECT_EQ(p->hit_test(kSurfTop, 578.0, 15.0), CsdHitZone::kMaximize);
}

TEST_F(CsdShmPluginTest, MinimizeButton) {
  const auto p = make_plugin();
  // center of minimize button: x = 554 + 6 = 560, y = 15
  EXPECT_EQ(p->hit_test(kSurfTop, 560.0, 15.0), CsdHitZone::kMinimize);
}

// ---------------------------------------------------------------------------
// Left panel
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginTest, LeftPanelMiddleIsResizeLeft) {
  const auto p = make_plugin();
  const double mid_y = static_cast<double>(left_h(*p)) / 2.0;
  EXPECT_EQ(p->hit_test(kSurfLeft, 2.0, mid_y), CsdHitZone::kResizeLeft);
}

TEST_F(CsdShmPluginTest, LeftPanelTopEdgeIsResizeTopLeft) {
  const auto p = make_plugin();
  EXPECT_EQ(p->hit_test(kSurfLeft, 2.0, 2.0), CsdHitZone::kResizeTopLeft);
}

TEST_F(CsdShmPluginTest, LeftPanelBottomEdgeIsResizeBottomLeft) {
  const auto p = make_plugin();
  const double near_bottom = static_cast<double>(left_h(*p)) - 2.0;
  EXPECT_EQ(p->hit_test(kSurfLeft, 2.0, near_bottom),
            CsdHitZone::kResizeBottomLeft);
}

// ---------------------------------------------------------------------------
// Right panel
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginTest, RightPanelMiddleIsResizeRight) {
  const auto p = make_plugin();
  const double mid_y = static_cast<double>(right_h(*p)) / 2.0;
  EXPECT_EQ(p->hit_test(kSurfRight, 2.0, mid_y), CsdHitZone::kResizeRight);
}

TEST_F(CsdShmPluginTest, RightPanelTopEdgeIsResizeTopRight) {
  const auto p = make_plugin();
  EXPECT_EQ(p->hit_test(kSurfRight, 2.0, 2.0), CsdHitZone::kResizeTopRight);
}

TEST_F(CsdShmPluginTest, RightPanelBottomEdgeIsResizeBottomRight) {
  const auto p = make_plugin();
  const double near_bottom = static_cast<double>(right_h(*p)) - 2.0;
  EXPECT_EQ(p->hit_test(kSurfRight, 2.0, near_bottom),
            CsdHitZone::kResizeBottomRight);
}

// ---------------------------------------------------------------------------
// Bottom panel
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginTest, BottomPanelMiddleIsResizeBottom) {
  const auto p = make_plugin();
  const double mid_x = static_cast<double>(bottom_w(*p)) / 2.0;
  EXPECT_EQ(p->hit_test(kSurfBottom, mid_x, 4.0), CsdHitZone::kResizeBottom);
}

TEST_F(CsdShmPluginTest, BottomPanelLeftEdgeIsResizeBottomLeft) {
  const auto p = make_plugin();
  EXPECT_EQ(p->hit_test(kSurfBottom, 2.0, 4.0), CsdHitZone::kResizeBottomLeft);
}

TEST_F(CsdShmPluginTest, BottomPanelRightEdgeIsResizeBottomRight) {
  const auto p = make_plugin();
  const double near_right = static_cast<double>(bottom_w(*p)) - 2.0;
  EXPECT_EQ(p->hit_test(kSurfBottom, near_right, 4.0),
            CsdHitZone::kResizeBottomRight);
}

// ---------------------------------------------------------------------------
// Edge: button hit-test boundary (just outside button -> title bar)
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginTest, JustLeftOfCloseButtonIsTitleBar) {
  const auto p = make_plugin();
  // the close button starts at x=590; x=589 is just outside -> title bar
  EXPECT_EQ(p->hit_test(kSurfTop, 589.0, 15.0), CsdHitZone::kTitleBar);
}

// ---------------------------------------------------------------------------
// Hidden / uninitialised plugin -> kNone
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginTest, HiddenPluginReturnsNone) {
  const auto p = make_plugin();
  set_visible(*p, false);
  set_initialised(*p, false);
  EXPECT_EQ(p->hit_test(kSurfTop, 300.0, 15.0), CsdHitZone::kNone);
}
