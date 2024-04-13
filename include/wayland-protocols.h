#ifndef INCLUDE_PROTOCOLS_H_
#define INCLUDE_PROTOCOLS_H_

#include "protocols/agl-shell-client-protocol.h"
#include "protocols/agl-shell-desktop-client-protocol.h"
#include "protocols/agl-screenshooter-client-protocol.h"

#include "protocols/ivi-wm-client-protocol.h"
#include "protocols/ivi-application-client-protocol.h"

#include "protocols/xdg-shell-client-protocol.h"

#if defined(WAYLAND_PROTOCOL_HAS_XDG_DECORATION)

#include "protocols/xdg-decoration-unstable-client-protocol.h"

#else
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;
#endif

#if defined(WAYLAND_PROTOCOL_HAS_PRESENTATION_TIME)

#include "protocols/presentation-time-client-protocol.h"

#else
struct wp_presentation;
#endif

#if defined(WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE)

#include "protocols/fractional-scale-v1-client-protocol.h"

#else
struct wp_fractional_scale_manager_v1;
struct wp_fractional_scale_v1;
#endif

#if defined(WAYLAND_PROTOCOL_HAS_TEARING_CONTROL)

#include "protocols/tearing-control-v1-client-protocol.h"

#else
struct wp_tearing_control_manager_v1;
#endif

#if defined(WAYLAND_PROTOCOL_HAS_VIEWPORTER)

#include "protocols/viewporter-client-protocol.h"

#else
struct wp_viewporter;
struct wp_viewport;
#endif

#if defined(WAYLAND_PROTOCOL_HAS_DRM_LEASE)

#include "protocols/drm-lease-v1-client-protocol.h"

#else
struct wp_drm_lease_connector_v1;
struct wp_drm_lease_device_v1;
struct wp_drm_lease_request_v1;
struct wp_drm_lease_v1;
#endif

#include <wayland-client-protocol.h>

#include "protocols/xdg-shell-client-protocol.h"

#endif //INCLUDE_PROTOCOLS_H_