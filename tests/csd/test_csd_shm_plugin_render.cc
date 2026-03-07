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

/// @file test_csd_shm_plugin_render.cc
///
/// Pixel-level rendering tests for CsdShmPlugin.
///
/// We inject a manually allocated pixel buffer into Panel structs via
/// `#define private public`, then call rendering helpers directly.
/// No Wayland compositor is required.

// Open private access to both Buffer and CsdShmPlugin in one pass.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define private public
#include "waypp/window/buffer.h"
#include "window/csd_shm_plugin.h"
#undef private

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// StubbedPanel — injects a vector<uint32_t> as the panel's SHM backing store
// without creating any Wayland objects.
// ---------------------------------------------------------------------------

struct StubbedPanel {
  std::vector<uint32_t> pixels;

  void init(CsdShmPlugin::Panel& panel, const int32_t w, const int32_t h) {
    pixels.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0u);
    panel.w = w;
    panel.h = h;
    panel.buffer = std::make_unique<Buffer>(nullptr /* wl_shm */);
    panel.buffer->shm_data_ = pixels.data();
    panel.buffer->width_ = w;
    panel.buffer->height_ = h;
    panel.buffer->size_ = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
  }

  static void cleanup(CsdShmPlugin::Panel& panel) {
    // Prevent Buffer::~Buffer() from calling munmap on our vector data.
    if (panel.buffer) {
      panel.buffer->shm_data_ = nullptr;
      panel.buffer->size_ = 0;
    }
  }
};

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class CsdShmPluginRender : public ::testing::Test {
 protected:
  CsdShmPlugin plugin_;
  StubbedPanel stub_top_;
  StubbedPanel stub_left_;  // reserved for future side-panel tests

  static constexpr int32_t kW = 608;  // frame width: content(600)+sides(4+4)
  static constexpr int32_t kH = 30;   // kTopH

  void SetUp() override {
    plugin_.initialised_ = true;
    plugin_.visible_ = true;
    plugin_.content_w_ = 600;
    plugin_.content_h_ = 400;
    stub_top_.init(plugin_.top_, kW, kH);
  }

  void TearDown() override {
    StubbedPanel::cleanup(plugin_.top_);
    StubbedPanel::cleanup(plugin_.left_);
  }

  [[nodiscard]] uint32_t pixel(const int32_t x, const int32_t y) const {
    return stub_top_.pixels[static_cast<size_t>(y) * static_cast<size_t>(kW) +
                            static_cast<size_t>(x)];
  }
};

// ---------------------------------------------------------------------------
// fill_rect tests
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginRender, FillRectFloodsRegion) {
  CsdShmPlugin::fill_rect(plugin_.top_, 0, 0, kW, kH, 0xFFAABBCCu);
  for (int32_t y = 0; y < kH; ++y) {
    for (int32_t x = 0; x < kW; ++x) {
      ASSERT_EQ(pixel(x, y), 0xFFAABBCCu) << "at (" << x << "," << y << ")";
    }
  }
}

TEST_F(CsdShmPluginRender, FillRectPartialRegion) {
  std::fill(stub_top_.pixels.begin(), stub_top_.pixels.end(), 0u);
  CsdShmPlugin::fill_rect(plugin_.top_, 10, 5, 20, 10, 0xFFFF0000u);
  for (int32_t y = 5; y < 15; ++y) {
    for (int32_t x = 10; x < 30; ++x) {
      EXPECT_EQ(pixel(x, y), 0xFFFF0000u)
          << "expected red at (" << x << "," << y << ")";
    }
  }
  EXPECT_EQ(pixel(9, 10), 0u);
  EXPECT_EQ(pixel(30, 10), 0u);
  EXPECT_EQ(pixel(15, 4), 0u);
  EXPECT_EQ(pixel(15, 15), 0u);
}

TEST_F(CsdShmPluginRender, FillRectOutOfBoundsIsNoop) {
  std::fill(stub_top_.pixels.begin(), stub_top_.pixels.end(), 0u);
  CsdShmPlugin::fill_rect(plugin_.top_, kW + 10, 0, 20, kH, 0xFFFFFFFFu);
  for (auto v : stub_top_.pixels)
    EXPECT_EQ(v, 0u);
}

TEST_F(CsdShmPluginRender, FillRectZeroSizeIsNoop) {
  std::fill(stub_top_.pixels.begin(), stub_top_.pixels.end(), 0u);
  CsdShmPlugin::fill_rect(plugin_.top_, 0, 0, 0, 0, 0xFFFFFFFFu);
  for (auto v : stub_top_.pixels)
    EXPECT_EQ(v, 0u);
}

// ---------------------------------------------------------------------------
// Glyph table sanity (constexpr static data, no Wayland needed)
// ---------------------------------------------------------------------------

TEST(CsdGlyphTable, SpaceGlyphIsAllZero) {
  for (int row = 0; row < CsdShmPlugin::kGlyphH; ++row) {
    EXPECT_EQ(CsdShmPlugin::kGlyphs[0][row], 0u) << "row " << row;
  }
}

TEST(CsdGlyphTable, ExclamationHasNonZeroRows) {
  bool any_set = false;
  for (int row = 0; row < CsdShmPlugin::kGlyphH; ++row) {
    if (CsdShmPlugin::kGlyphs[1][row] != 0u) {
      any_set = true;
      break;
    }
  }
  EXPECT_TRUE(any_set);
}

TEST(CsdGlyphTable, AllGlyphsWithinFiveBits) {
  for (int gi = 0; gi < 95; ++gi) {
    for (int row = 0; row < CsdShmPlugin::kGlyphH; ++row) {
      EXPECT_EQ(CsdShmPlugin::kGlyphs[gi][row] & ~0x1Fu, 0u)
          << "glyph[" << gi << "][" << row << "] has bits beyond column 4";
    }
  }
}

TEST(CsdGlyphTable, TableCoversAllPrintableAscii) {
  EXPECT_EQ(std::size(CsdShmPlugin::kGlyphs), 95u);
}

// ---------------------------------------------------------------------------
// paint_title — verify text pixels appear in the top panel
// ---------------------------------------------------------------------------

TEST_F(CsdShmPluginRender, PaintTitleWritesNonZeroPixels) {
  std::fill(stub_top_.pixels.begin(), stub_top_.pixels.end(), 0u);
  plugin_.title_ = "Hi";
  plugin_.paint_title("Hi");
  bool found = false;
  for (auto v : stub_top_.pixels) {
    if (v == CsdShmPlugin::kColText) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found)
      << "Expected at least one white pixel after paint_title(\"Hi\")";
}

TEST_F(CsdShmPluginRender, PaintTitleEmptyStringIsNoop) {
  std::fill(stub_top_.pixels.begin(), stub_top_.pixels.end(), 0u);
  plugin_.paint_title("");
  for (auto v : stub_top_.pixels)
    EXPECT_EQ(v, 0u);
}
