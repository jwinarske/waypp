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

/// Hit-test zones returned by CsdPlugin::hit_test().
///
/// The resize edge values intentionally match xdg_toplevel_resize_edge so they
/// can be cast directly when calling XdgTopLevel::resize():
///
///   if (zone >= CsdHitZone::kResizeTop)
///     toplevel_->resize(seat, serial,
///                       static_cast<xdg_toplevel_resize_edge>(zone));
///
/// Values >= 100 are decoration-specific zones (title bar, buttons) that have
/// no xdg_toplevel_resize_edge equivalent.
enum class CsdHitZone {
  /// Pointer is over the application content area — pass events through.
  kNone = 0,

  // -----------------------------------------------------------------------
  // Resize edges — values == xdg_toplevel_resize_edge enum members.
  // -----------------------------------------------------------------------
  kResizeTop = 1,
  kResizeBottom = 2,
  kResizeLeft = 4,
  kResizeTopLeft = 5,
  kResizeBottomLeft = 6,
  kResizeRight = 8,
  kResizeTopRight = 9,
  kResizeBottomRight = 10,

  // -----------------------------------------------------------------------
  // Decoration-specific zones (no xdg_toplevel_resize_edge equivalent).
  // -----------------------------------------------------------------------

  /// Title-bar strip — drag initiates an interactive move.
  kTitleBar = 100,

  /// Close button.
  kClose = 101,

  /// Maximize / restore button.
  kMaximize = 102,

  /// Minimize button.
  kMinimize = 103,
};
