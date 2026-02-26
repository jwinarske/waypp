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

#ifndef INCLUDE_WAYPP_CONFIG_H_
#define INCLUDE_WAYPP_CONFIG_H_

/// Wayland interface version selection
static constexpr uint32_t kWlCompositorMinVersion = UINT32_C(5);
static constexpr uint32_t kWlSubcompositorMinVersion = UINT32_C(1);
static constexpr uint32_t kWlShmMinVersion = UINT32_C(1);
static constexpr uint32_t kWlSeatMinVersion = UINT32_C(8);
static constexpr uint32_t kWlOutputMinVersion = UINT32_C(4);
static constexpr uint32_t kPresentationTimeMinVersion = UINT32_C(1);

static constexpr uint32_t kXdgWmBaseMinVersion = UINT32_C(6);
static constexpr uint32_t kXdgActivationV1MinVersion = UINT32_C(1);
static constexpr uint32_t kXdgOutputManagerMinVersion = UINT32_C(1);
static constexpr uint32_t kXdgDecorationManagerMinVersion = UINT32_C(1);

static constexpr uint32_t kAglShellMinVersion = UINT32_C(11);

static constexpr uint32_t kIviWmMinVersion = UINT32_C(1);
static constexpr uint32_t kDrmLeaseDeviceV1MinVersion = UINT32_C(1);

static constexpr uint32_t kViewporterMinVersion = UINT32_C(1);
static constexpr uint32_t kTearingControlManagerMinVersion = UINT32_C(1);
static constexpr uint32_t kFractionalScaleManagerMinVersion = UINT32_C(1);
static constexpr uint32_t kWestonCaptureV1MinVersion = UINT32_C(1);
static constexpr uint32_t kIdleInhibitManagerV1MinVersion = UINT32_C(1);
static constexpr uint32_t kCursorShapeManagerMinVersion = UINT32_C(1);

static constexpr uint32_t kPointerGesturesV1MinVersion = UINT32_C(3);
static constexpr uint32_t kPointerConstraintsV1MinVersion = UINT32_C(1);
static constexpr uint32_t kRelativePointerManagerV1MinVersion = UINT32_C(1);

static constexpr uint32_t kPrimarySelectionDeviceManagerV1MinVersion =
    UINT32_C(1);

#endif  // INCLUDE_WAYPP_CONFIG_H_
