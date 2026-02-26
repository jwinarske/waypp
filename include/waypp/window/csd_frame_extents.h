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

#include <cstdint>

/// Border widths claimed by client-side decorations in logical pixels.
///
/// All fields are zero when the compositor provides server-side decorations or
/// when CSD is disabled, allowing callers to use the extents unconditionally
/// without special-casing the SSD path.
///
/// Coordinate convention (matches libdecor):
///   - The *frame* surface encompasses both the content area and the borders.
///   - Content starts at (left, top) within the frame surface.
///   - The total frame dimensions are content + left+right / top+bottom.
struct CsdFrameExtents {
  int32_t left{};
  int32_t right{};
  int32_t top{};
  int32_t bottom{};

  /// Total frame width for a given content width.
  [[nodiscard]] constexpr int32_t frame_width(int32_t content_w) const {
    return content_w + left + right;
  }

  /// Total frame height for a given content height.
  [[nodiscard]] constexpr int32_t frame_height(int32_t content_h) const {
    return content_h + top + bottom;
  }

  /// X offset of the content area within the frame surface.
  [[nodiscard]] constexpr int32_t content_x() const { return left; }

  /// Y offset of the content area within the frame surface.
  [[nodiscard]] constexpr int32_t content_y() const { return top; }

  /// True when decorations are visible (floating state: not fullscreen,
  /// not maximized, not tiled).  Mirrors libdecor_frame_is_floating().
  [[nodiscard]] constexpr bool is_floating() const { return top > 0; }

  /// True when all borders are zero (SSD active or CSD disabled).
  [[nodiscard]] constexpr bool is_empty() const {
    return left == 0 && right == 0 && top == 0 && bottom == 0;
  }

  [[nodiscard]] constexpr bool operator==(const CsdFrameExtents& o) const {
    return left == o.left && right == o.right && top == o.top &&
           bottom == o.bottom;
  }

  [[nodiscard]] constexpr bool operator!=(const CsdFrameExtents& o) const {
    return !(*this == o);
  }
};
