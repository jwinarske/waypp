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

#define ENABLE_XDG_CLIENT 1
#define ENABLE_AGL_SHELL_CLIENT 1
#define ENABLE_IVI_SHELL_CLIENT 0
#define ENABLE_DRM_LEASE_CLIENT 0

#define ENABLE_EGL 1

#define HAS_WAYLAND_PROTOCOL_XDG_SHELL 1
#define HAS_WAYLAND_PROTOCOL_AGL_SHELL 1
#define HAS_WAYLAND_PROTOCOL_AGL_SHELL_DESKTOP 1
#define HAS_WAYLAND_PROTOCOL_AGL_SCREENSHOOTER 1
#define HAS_WAYLAND_PROTOCOL_DRM_LEASE_V1 0
#define HAS_WAYLAND_PROTOCOL_PRESENTATION_TIME 1
#define HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1 1
#define HAS_WAYLAND_PROTOCOL_VIEWPORTER 1
#define HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1 1
#define HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1 1
#define HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1 1
#define HAS_WAYLAND_PROTOCOL_XDG_ACTIVATION_V1 1
#define HAS_WAYLAND_PROTOCOL_LINUX_DMABUF_V1 1
#define HAS_WAYLAND_PROTOCOL_WESTON_OUTPUT_CAPTURE 1
#define HAS_WAYLAND_PROTOCOL_CURSOR_SHAPE_V1 1
#define HAS_WAYLAND_PROTOCOL_POINTER_CONSTRAINTS_UNSTABLE_V1 1
#define HAS_WAYLAND_PROTOCOL_POINTER_GESTURES_UNSTABLE_V1 1
#define HAS_WAYLAND_PROTOCOL_RELATIVE_POINTER_UNSTABLE_V1 1
#define HAS_WAYLAND_PROTOCOL_IDLE_INHIBIT_UNSTABLE_V1 1
#define HAS_WAYLAND_PROTOCOL_PRIMARY_SELECTION_UNSTABLE_V1 1

#define HAVE_MEMFD_CREATE 1
#define HAVE_POSIX_FALLOCATE 1
#define HAVE_MKOSTEMP 1

#define BUILD_WAYPP_STANDALONE 1

#if ENABLE_XDG_CLIENT
#include "xdg-shell-client-protocol.h"
#endif

#if ENABLE_AGL_SHELL_CLIENT
#include "agl-shell-client-protocol.h"
#include "agl-shell-desktop-client-protocol.h"
#include "agl-screenshooter-client-protocol.h"
#endif

#if ENABLE_IVI_SHELL_CLIENT
#include "ivi-wm-client-protocol.h"
#include "ivi-application-client-protocol.h"
#endif

#if HAS_WAYLAND_PROTOCOL_DRM_LEASE_V1
#include "drm-lease-v1-client-protocol.h"
#endif

#if HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1
#include "xdg-decoration-unstable-v1-client-protocol.h"
#endif

#include "presentation-time-client-protocol.h"

#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1
#include "fractional-scale-v1-client-protocol.h"
#endif

#if HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1
#include "tearing-control-v1-client-protocol.h"
#endif

#if HAS_WAYLAND_PROTOCOL_VIEWPORTER
#include "viewporter-client-protocol.h"
#endif

#if HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1
#include "xdg-output-unstable-v1-client-protocol.h"
#endif

#if HAS_WAYLAND_PROTOCOL_XDG_ACTIVATION_V1
#include "xdg-activation-v1-client-protocol.h"
#endif
#if HAS_WAYLAND_PROTOCOL_LINUX_DMABUF_V1
#include "linux-dmabuf-v1-client-protocol.h"
#endif
#if HAS_WAYLAND_PROTOCOL_WESTON_OUTPUT_CAPTURE
#include "weston-output-capture-client-protocol.h"
#endif
#if HAS_WAYLAND_PROTOCOL_CURSOR_SHAPE_V1
#include "cursor-shape-v1-client-protocol.h"
#endif
#if HAS_WAYLAND_PROTOCOL_POINTER_CONSTRAINTS_UNSTABLE_V1
#include "pointer-constraints-unstable-v1-client-protocol.h"
#endif
#if HAS_WAYLAND_PROTOCOL_POINTER_GESTURES_UNSTABLE_V1
#include "pointer-gestures-unstable-v1-client-protocol.h"
#endif
#if HAS_WAYLAND_PROTOCOL_RELATIVE_POINTER_UNSTABLE_V1
#include "relative-pointer-unstable-v1-client-protocol.h"
#endif
#if HAS_WAYLAND_PROTOCOL_IDLE_INHIBIT_UNSTABLE_V1
#include "idle-inhibit-unstable-v1-client-protocol.h"
#endif
#if HAS_WAYLAND_PROTOCOL_PRIMARY_SELECTION_UNSTABLE_V1
#include "primary-selection-unstable-v1-client-protocol.h"
#endif

#endif //INCLUDE_PROTOCOLS_H_
