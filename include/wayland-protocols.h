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

#ifndef INCLUDE_PROTOCOLS_H_
#define INCLUDE_PROTOCOLS_H_

#include <wayland-client-protocol.h>

#if defined(ENABLE_XDG_CLIENT)

#include "xdg-shell-client-protocol.h"

#endif

#if defined(ENABLE_AGL_SHELL_CLIENT)
#include "agl-shell-client-protocol.h"
#include "agl-shell-desktop-client-protocol.h"
#include "agl-screenshooter-client-protocol.h"
#endif

#if defined(ENABLE_IVI_SHELL_CLIENT)
#include "ivi-wm-client-protocol.h"
#include "ivi-application-client-protocol.h"
#endif

#if defined(HAS_WAYLAND_PROTOCOL_DRM_LEASE_V1)

#include "drm-lease-v1-client-protocol.h"

#endif

#if defined(HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1)

#include "xdg-decoration-unstable-v1-client-protocol.h"

#else
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;
#endif

#include "presentation-time-client-protocol.h"

#if defined(HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1)

#include "fractional-scale-v1-client-protocol.h"

#else
struct wp_fractional_scale_manager_v1;
struct wp_fractional_scale_v1;
#endif

#if defined(HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1)

#include "tearing-control-v1-client-protocol.h"

#else
struct wp_tearing_control_manager_v1;
#endif

#if defined(HAS_WAYLAND_PROTOCOL_VIEWPORTER)

#include "viewporter-client-protocol.h"

#else
struct wp_viewporter;
struct wp_viewport;
#endif

#if defined(HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1)

#include "xdg-output-unstable-v1-client-protocol.h"

#else
struct zxdg_output_manager_v1;
struct zxdg_output_v1;
#endif

#endif //INCLUDE_PROTOCOLS_H_