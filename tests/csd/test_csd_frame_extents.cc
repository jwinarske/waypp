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

/// @file test_csd_frame_extents.cc
/// Unit tests for CsdFrameExtents — pure arithmetic, no Wayland dependency.

#include <gtest/gtest.h>

#include "waypp/window/csd_frame_extents.h"

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST(CsdFrameExtents, DefaultIsZero) {
  constexpr CsdFrameExtents e;
  EXPECT_EQ(e.left, 0);
  EXPECT_EQ(e.right, 0);
  EXPECT_EQ(e.top, 0);
  EXPECT_EQ(e.bottom, 0);
}

TEST(CsdFrameExtents, DefaultIsEmpty) {
  constexpr CsdFrameExtents e;
  EXPECT_TRUE(e.is_empty());
}

TEST(CsdFrameExtents, DefaultIsNotFloating) {
  constexpr CsdFrameExtents e;
  EXPECT_FALSE(e.is_floating());
}

// ---------------------------------------------------------------------------
// Aggregate initialisation
// ---------------------------------------------------------------------------

TEST(CsdFrameExtents, AggregateInit) {
  constexpr CsdFrameExtents e{4, 4, 30, 8};
  EXPECT_EQ(e.left, 4);
  EXPECT_EQ(e.right, 4);
  EXPECT_EQ(e.top, 30);
  EXPECT_EQ(e.bottom, 8);
}

// ---------------------------------------------------------------------------
// frame_width / frame_height
// ---------------------------------------------------------------------------

TEST(CsdFrameExtents, FrameWidthAddsLeftAndRight) {
  constexpr CsdFrameExtents e{4, 4, 30, 8};
  EXPECT_EQ(e.frame_width(600), 608);
}

TEST(CsdFrameExtents, FrameHeightAddsTopAndBottom) {
  constexpr CsdFrameExtents e{4, 4, 30, 8};
  EXPECT_EQ(e.frame_height(400), 438);
}

TEST(CsdFrameExtents, FrameWidthZeroExtents) {
  constexpr CsdFrameExtents e{};
  EXPECT_EQ(e.frame_width(250), 250);
}

TEST(CsdFrameExtents, FrameHeightZeroExtents) {
  constexpr CsdFrameExtents e{};
  EXPECT_EQ(e.frame_height(250), 250);
}

TEST(CsdFrameExtents, FrameDimensionsAsymmetric) {
  constexpr CsdFrameExtents e{2, 6, 30, 8};
  EXPECT_EQ(e.frame_width(100), 108);   // 2+6=8
  EXPECT_EQ(e.frame_height(100), 138);  // 30+8=38
}

// ---------------------------------------------------------------------------
// content_x / content_y
// ---------------------------------------------------------------------------

TEST(CsdFrameExtents, ContentXEqualsLeft) {
  constexpr CsdFrameExtents e{4, 4, 30, 8};
  EXPECT_EQ(e.content_x(), 4);
}

TEST(CsdFrameExtents, ContentYEqualsTop) {
  constexpr CsdFrameExtents e{4, 4, 30, 8};
  EXPECT_EQ(e.content_y(), 30);
}

// ---------------------------------------------------------------------------
// is_floating
// ---------------------------------------------------------------------------

TEST(CsdFrameExtents, IsFloatingWhenTopPositive) {
  constexpr CsdFrameExtents e{4, 4, 30, 8};
  EXPECT_TRUE(e.is_floating());
}

TEST(CsdFrameExtents, IsNotFloatingWhenTopZero) {
  constexpr CsdFrameExtents e{0, 0, 0, 0};
  EXPECT_FALSE(e.is_floating());
}

TEST(CsdFrameExtents, IsNotFloatingWhenTopZeroButSidesNonZero) {
  // Decorations hidden in maximized state — top=0 even if sides non-zero.
  constexpr CsdFrameExtents e{4, 4, 0, 0};
  EXPECT_FALSE(e.is_floating());
}

// ---------------------------------------------------------------------------
// is_empty
// ---------------------------------------------------------------------------

TEST(CsdFrameExtents, IsEmptyAllZero) {
  EXPECT_TRUE((CsdFrameExtents{0, 0, 0, 0}.is_empty()));
}

TEST(CsdFrameExtents, IsNotEmptyIfAnyNonZero) {
  EXPECT_FALSE((CsdFrameExtents{1, 0, 0, 0}.is_empty()));
  EXPECT_FALSE((CsdFrameExtents{0, 1, 0, 0}.is_empty()));
  EXPECT_FALSE((CsdFrameExtents{0, 0, 1, 0}.is_empty()));
  EXPECT_FALSE((CsdFrameExtents{0, 0, 0, 1}.is_empty()));
}

// ---------------------------------------------------------------------------
// Equality operators
// ---------------------------------------------------------------------------

TEST(CsdFrameExtents, EqualityTrue) {
  constexpr CsdFrameExtents a{4, 4, 30, 8};
  constexpr CsdFrameExtents b{4, 4, 30, 8};
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(CsdFrameExtents, EqualityFalseLeftDiffers) {
  constexpr CsdFrameExtents a{4, 4, 30, 8};
  constexpr CsdFrameExtents b{5, 4, 30, 8};
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(CsdFrameExtents, EqualityFalseRightDiffers) {
  constexpr CsdFrameExtents a{4, 4, 30, 8};
  constexpr CsdFrameExtents b{4, 5, 30, 8};
  EXPECT_FALSE(a == b);
}

TEST(CsdFrameExtents, EqualityFalseTopDiffers) {
  constexpr CsdFrameExtents a{4, 4, 30, 8};
  constexpr CsdFrameExtents b{4, 4, 31, 8};
  EXPECT_FALSE(a == b);
}

TEST(CsdFrameExtents, EqualityFalseBottomDiffers) {
  constexpr CsdFrameExtents a{4, 4, 30, 8};
  constexpr CsdFrameExtents b{4, 4, 30, 9};
  EXPECT_FALSE(a == b);
}

TEST(CsdFrameExtents, SelfEquality) {
  constexpr CsdFrameExtents a{4, 4, 30, 8};
  EXPECT_TRUE(a == a);
}

// ---------------------------------------------------------------------------
// Round-trip: frame → content dimensions
// ---------------------------------------------------------------------------

TEST(CsdFrameExtents, RoundTripWidth) {
  constexpr CsdFrameExtents e{4, 4, 30, 8};
  constexpr int32_t content_w = 600;
  constexpr int32_t frame_w = e.frame_width(content_w);
  // content recovered by subtracting left + right
  EXPECT_EQ(frame_w - e.left - e.right, content_w);
}

TEST(CsdFrameExtents, RoundTripHeight) {
  constexpr CsdFrameExtents e{4, 4, 30, 8};
  constexpr int32_t content_h = 400;
  constexpr int32_t frame_h = e.frame_height(content_h);
  EXPECT_EQ(frame_h - e.top - e.bottom, content_h);
}
