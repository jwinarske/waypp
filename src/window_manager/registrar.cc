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

#include "waypp/window_manager/registrar.h"

#include "logging/logging.h"
#include "waypp/window_manager/output.h"

Registrar::Registrar(wl_display* wl_display,
                     const unsigned long ext_interface_count,
                     const RegistrarCallback* ext_interface_data,
                     const bool disable_cursor)
    : wl_display_(wl_display),
      wl_registry_(wl_display_get_registry(wl_display)),
      disable_cursor_(disable_cursor) {
  DLOG_TRACE("++Registrar::Registrar()");

  registrar_global_ =
      std::make_unique<std::map<std::string, RegistrarGlobalCallback>>();
  registrar_global_remove_ =
      std::make_unique<std::map<uint32_t, RegistrarGlobalRemoveCallback>>();

  *registrar_global_ = {
      {wl_compositor_interface.name, handle_interface_compositor},
      {wl_subcompositor_interface.name, handle_interface_subcompositor},
      {wl_shm_interface.name, handle_interface_shm},
      {wl_seat_interface.name, handle_interface_seat},
      {wl_output_interface.name, handle_interface_output},
      {weston_capture_v1_interface.name, handle_interface_weston_capture_v1}
#if ENABLE_XDG_CLIENT
      ,
      {xdg_wm_base_interface.name, handle_interface_xdg_wm_base}
#endif
#if ENABLE_AGL_SHELL_CLIENT
      ,
      {agl_shell_interface.name, handle_interface_agl_shell}
#endif
#if ENABLE_IVI_SHELL_CLIENT
      ,
      {ivi_wm_interface.name, handle_interface_ivi_wm}
#endif
#if ENABLE_DRM_LEASE_CLIENT
          .{wp_drm_lease_device_v1_interface.name,
            handle_interface_drm_lease_device_v1}
#endif
#if HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1
      ,
      {zxdg_decoration_manager_v1_interface.name,
       handle_interface_zxdg_decoration},
      {zxdg_toplevel_decoration_v1_interface.name,
       handle_interface_zxdg_toplevel_decoration}
#endif
#if HAS_WAYLAND_PROTOCOL_PRESENTATION_TIME
      ,
      {wp_presentation_interface.name, handle_interface_presentation}
#endif
#if HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1
      ,
      {wp_tearing_control_manager_v1_interface.name,
       handle_interface_tearing_control_manager}
#endif
#if HAS_WAYLAND_PROTOCOL_VIEWPORTER
      ,
      {wp_viewporter_interface.name, handle_interface_viewporter}
#endif
#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1
      ,
      {wp_fractional_scale_manager_v1_interface.name,
       handle_interface_fractional_scale_manager}
#endif
#if HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1
      ,
      {zxdg_output_manager_v1_interface.name,
       handle_interface_xdg_output_unstable_v1}
#endif
#if HAS_WAYLAND_PROTOCOL_IDLE_INHIBIT_UNSTABLE_V1
      ,
      {zwp_idle_inhibit_manager_v1_interface.name,
       handle_interface_zwp_idle_inhibit_manager_v1}
#endif
#if HAS_WAYLAND_PROTOCOL_XDG_ACTIVATION_V1
      ,
      {xdg_activation_v1_interface.name, handle_interface_xdg_activation_v1}
#endif
#if HAS_WAYLAND_PROTOCOL_POINTER_GESTURES_UNSTABLE_V1
      ,
      {zwp_pointer_gestures_v1_interface.name,
       handle_interface_zwp_pointer_gestures_v1}
#endif

#if HAS_WAYLAND_PROTOCOL_POINTER_CONSTRAINTS_UNSTABLE_V1
      ,
      {zwp_pointer_constraints_v1_interface.name,
       handle_interface_zwp_pointer_constraints_v1}
#endif

#if HAS_WAYLAND_PROTOCOL_RELATIVE_POINTER_UNSTABLE_V1
      ,
      {zwp_relative_pointer_manager_v1_interface.name,
       handle_interface_zwp_relative_pointer_manager_v1}
#endif
#if HAS_WAYLAND_PROTOCOL_PRIMARY_SELECTION_UNSTABLE_V1
      ,
      {zwp_primary_selection_device_manager_v1_interface.name,
       handle_interface_zwp_primary_selection_device_manager_v1}
#endif
  };

  /// Add external interfaces
  if (ext_interface_count) {
    for (unsigned long i = 0; i < ext_interface_count; i++) {
      (*registrar_global_)[ext_interface_data[i].interface] =
          ext_interface_data[i].global_callback;
    }
  }

  wl_registry_add_listener(wl_registry_, &listener_, this);
  wl_display_roundtrip(wl_display_);
  DLOG_TRACE("--Registrar::Registrar()");
}

Registrar::~Registrar() {
  DLOG_TRACE("++Registrar::~Registrar()");
  outputs_.clear();
  seats_.clear();

  if (wl_shm_) {
    DLOG_TRACE("[Registrar] wl_shm_destroy(wl_shm_)");
    wl_shm_destroy(wl_shm_);
    wl_shm_formats_.clear();
  }

  if (wl_compositor_) {
    DLOG_TRACE("[Registrar] wl_compositor_destroy(wl_compositor_)");
    wl_compositor_destroy(wl_compositor_);
  }

  if (wl_subcompositor_) {
    LOG_TRACE("[Registrar] wl_subcompositor_destroy(wl_subcompositor_)");
    wl_subcompositor_destroy(wl_subcompositor_);
  }

#if ENABLE_AGL_SHELL_CLIENT
  if (agl_shell_) {
    LOG_TRACE("[Registrar] agl_shell_destroy(agl_shell_)");
    agl_shell_destroy(agl_shell_);
  }
#endif

#if ENABLE_XDG_CLIENT
  if (xdg_wm_base_) {
    LOG_TRACE("[Registrar] xdg_wm_base_destroy(xdg_wm_base_)");
    xdg_wm_base_destroy(xdg_wm_base_);
  }
#endif

  if (presentation_time_.wp_presentation) {
    LOG_TRACE(
        "[Registrar] "
        "wp_presentation_destroy(presentation_time_.wp_presentation)");
    wp_presentation_destroy(presentation_time_.wp_presentation);
  }

#if HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1
  if (zxdg_toplevel_decoration_v1_) {
    LOG_TRACE(
        "[Registrar] "
        "zxdg_toplevel_decoration_v1_destroy(zxdg_toplevel_decoration_v1_)");
    zxdg_toplevel_decoration_v1_destroy(zxdg_toplevel_decoration_v1_);
  }
#endif
#if HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1
  if (zxdg_decoration_manager_v1_) {
    LOG_TRACE(
        "[Registrar] "
        "zxdg_decoration_manager_v1_destroy(zxdg_decoration_manager_v1_)");
    zxdg_decoration_manager_v1_destroy(zxdg_decoration_manager_v1_);
  }
#endif
#if HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1
  if (wp_tearing_control_manager_) {
    LOG_TRACE(
        "[Registrar] "
        "wp_tearing_control_manager_v1_destroy(wp_tearing_control_manager_)");
    wp_tearing_control_manager_v1_destroy(wp_tearing_control_manager_);
  }
#endif
#if HAS_WAYLAND_PROTOCOL_VIEWPORTER
  if (wp_viewporter_) {
    LOG_TRACE("[Registrar] wp_viewporter_destroy(wp_viewporter_)");
    wp_viewporter_destroy(wp_viewporter_);
  }
#endif
#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1
  if (fractional_scale_manager_) {
    LOG_TRACE(
        "[Registrar] "
        "wp_fractional_scale_manager_v1_destroy(fractional_scale_manager_)");
    wp_fractional_scale_manager_v1_destroy(fractional_scale_manager_);
  }
#endif
#if HAS_WAYLAND_PROTOCOL_IDLE_INHIBIT_UNSTABLE_V1
  if (zwp_idle_inhibit_manager_v1_) {
    LOG_TRACE(
        "[Registrar] "
        "zwp_idle_inhibit_manager_v1_destroy(zwp_idle_inhibit_manager_v1_)");
    zwp_idle_inhibit_manager_v1_destroy(zwp_idle_inhibit_manager_v1_);
  }
#endif
#if HAS_WAYLAND_PROTOCOL_POINTER_GESTURES_UNSTABLE_V1
  if (zwp_pointer_gestures_v1_) {
    LOG_TRACE(
        "[Registrar] "
        "zwp_pointer_gestures_v1_destroy(zwp_pointer_gestures_v1_)");
    zwp_pointer_gestures_v1_destroy(zwp_pointer_gestures_v1_);
  }
#endif
#if HAS_WAYLAND_PROTOCOL_POINTER_CONSTRAINTS_UNSTABLE_V1
  if (zwp_pointer_constraints_v1_) {
    LOG_TRACE(
        "[Registrar] "
        "zwp_pointer_constraints_v1_destroy(zwp_pointer_constraints_v1_)");
    zwp_pointer_constraints_v1_destroy(zwp_pointer_constraints_v1_);
  }
#endif
#if HAS_WAYLAND_PROTOCOL_RELATIVE_POINTER_UNSTABLE_V1
  if (zwp_relative_pointer_manager_v1_) {
    LOG_TRACE(
        "[Registrar] "
        "zwp_relative_pointer_manager_v1_destroy(zwp_relative_pointer_manager_"
        "v1_)");
    zwp_relative_pointer_manager_v1_destroy(zwp_relative_pointer_manager_v1_);
  }
#endif
#if HAS_WAYLAND_PROTOCOL_PRIMARY_SELECTION_UNSTABLE_V1
  if (zwp_primary_selection_device_manager_v1_) {
    LOG_TRACE(
        "[Registrar] "
        "zwp_primary_selection_device_manager_v1_destroy(zwp_primary_selection_"
        "device_manager_v1_)");
    zwp_primary_selection_device_manager_v1_destroy(
        zwp_primary_selection_device_manager_v1_);
  }
#endif

  if (wl_registry_) {
    LOG_TRACE("[Registrar] wl_registry_destroy(wl_registry_)");
    wl_registry_destroy(wl_registry_);
  }
  LOG_TRACE("--Registrar::~Registrar()");
}

void Registrar::registry_handle_global(void* data,
                                       wl_registry* registry,
                                       uint32_t name,
                                       const char* interface,
                                       uint32_t version) {
  LOG_TRACE("++Registrar::registry_handle_global()\t\n\t{}: {}", interface,
            version);
  auto registrar = static_cast<Registrar*>(data);
  auto found = (*registrar->registrar_global_).find(interface);
  if (found != (*registrar->registrar_global_).end()) {
    // Execute the process function for the found interface.
    found->second(registrar, registry, name, interface, version);
  }
  LOG_TRACE("--Registrar::registry_handle_global()");
}

void Registrar::registry_handle_global_remove(void* data,
                                              wl_registry* reg,
                                              uint32_t id) {
  LOG_TRACE("++Registrar::registry_handle_global_remove()");
  auto registrar = static_cast<Registrar*>(data);

  auto found = (*registrar->registrar_global_remove_).find(id);
  if (found != (*registrar->registrar_global_remove_).end()) {
    found->second(registrar, reg,
                  id);  // Execute the process function for the found interface.
  }
  LOG_TRACE("--Registrar::registry_handle_global_remove()");
}

wl_output_transform Registrar::get_output_buffer_transform(
    wl_output* wl_output) {
  for (auto& output : outputs_) {
    if (wl_output == output.first) {
      return output.second->get_transform();
    }
  }
  return WL_OUTPUT_TRANSFORM_NORMAL;
}

int32_t Registrar::get_output_buffer_scale(wl_output* wl_output) {
  int32_t scale = 1;
  for (auto& output : outputs_) {
    if (wl_output == output.first) {
      scale = output.second->get_scale_factor();
      break;
    }
  }
  return scale;
}

void Registrar::shm_format(void* data, wl_shm* wl_shm, uint32_t format) {
  LOG_TRACE("++Registrar::shm_format()");
  auto obj = static_cast<Registrar*>(data);
  if (obj->wl_shm_ != wl_shm) {
    return;
  }
  obj->wl_shm_formats_.push_back(format);
  LOG_TRACE("--Registrar::shm_format()");
}

const char* Registrar::shm_format_to_text(wl_shm_format format) {
  switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
      return "ARGB8888";
    case WL_SHM_FORMAT_XRGB8888:
      return "XRGB8888";
    case WL_SHM_FORMAT_C8:
      return "C8";
    case WL_SHM_FORMAT_RGB332:
      return "RGB332";
    case WL_SHM_FORMAT_BGR233:
      return "BGR233";
    case WL_SHM_FORMAT_XRGB4444:
      return "XRGB4444";
    case WL_SHM_FORMAT_XBGR4444:
      return "XBGR4444";
    case WL_SHM_FORMAT_RGBX4444:
      return "RGBX4444";
    case WL_SHM_FORMAT_BGRX4444:
      return "BGRX4444";
    case WL_SHM_FORMAT_ARGB4444:
      return "ARGB4444";
    case WL_SHM_FORMAT_ABGR4444:
      return "ABGR4444";
    case WL_SHM_FORMAT_RGBA4444:
      return "RGBA4444:";
    case WL_SHM_FORMAT_BGRA4444:
      return "BGRA4444";
    case WL_SHM_FORMAT_XRGB1555:
      return "XRGB1555";
    case WL_SHM_FORMAT_XBGR1555:
      return "XBGR1555";
    case WL_SHM_FORMAT_RGBX5551:
      return "RGBX5551";
    case WL_SHM_FORMAT_BGRX5551:
      return "BGRX5551";
    case WL_SHM_FORMAT_ARGB1555:
      return "ARGB1555";
    case WL_SHM_FORMAT_ABGR1555:
      return "ABGR1555";
    case WL_SHM_FORMAT_RGBA5551:
      return "RGBA5551";
    case WL_SHM_FORMAT_BGRA5551:
      return "BGRA5551";
    case WL_SHM_FORMAT_RGB565:
      return "RGB565";
    case WL_SHM_FORMAT_BGR565:
      return "BGR565";
    case WL_SHM_FORMAT_RGB888:
      return "RGB888";
    case WL_SHM_FORMAT_BGR888:
      return "BGR888";
    case WL_SHM_FORMAT_XBGR8888:
      return "XBGR8888";
    case WL_SHM_FORMAT_RGBX8888:
      return "RGBX8888";
    case WL_SHM_FORMAT_BGRX8888:
      return "BGRX8888";
    case WL_SHM_FORMAT_ABGR8888:
      return "ABGR8888";
    case WL_SHM_FORMAT_RGBA8888:
      return "RGBA8888";
    case WL_SHM_FORMAT_BGRA8888:
      return "BGRA8888";
    case WL_SHM_FORMAT_XRGB2101010:
      return "XRGB2101010";
    case WL_SHM_FORMAT_XBGR2101010:
      return "XBGR2101010";
    case WL_SHM_FORMAT_RGBX1010102:
      return "RGBX1010102";
    case WL_SHM_FORMAT_BGRX1010102:
      return "BGRX1010102";
    case WL_SHM_FORMAT_ARGB2101010:
      return "ARGB2101010";
    case WL_SHM_FORMAT_ABGR2101010:
      return "ABGR2101010";
    case WL_SHM_FORMAT_RGBA1010102:
      return "RGBA1010102";
    case WL_SHM_FORMAT_BGRA1010102:
      return "BGRA1010102";
    case WL_SHM_FORMAT_YUYV:
      return "YUYV";
    case WL_SHM_FORMAT_YVYU:
      return "YVYU";
    case WL_SHM_FORMAT_UYVY:
      return "UYVY";
    case WL_SHM_FORMAT_VYUY:
      return "VYUY";
    case WL_SHM_FORMAT_AYUV:
      return "AYUV";
    case WL_SHM_FORMAT_NV12:
      return "NV12";
    case WL_SHM_FORMAT_NV21:
      return "NV21";
    case WL_SHM_FORMAT_NV16:
      return "NV16";
    case WL_SHM_FORMAT_NV61:
      return "NV61";
    case WL_SHM_FORMAT_YUV410:
      return "YUV410";
    case WL_SHM_FORMAT_YVU410:
      return "YVU410";
    case WL_SHM_FORMAT_YUV411:
      return "YUV411";
    case WL_SHM_FORMAT_YVU411:
      return "YVU411";
    case WL_SHM_FORMAT_YUV420:
      return "YUV420";
    case WL_SHM_FORMAT_YVU420:
      return "YVU420";
    case WL_SHM_FORMAT_YUV422:
      return "YUV422";
    case WL_SHM_FORMAT_YVU422:
      return "YVU422";
    case WL_SHM_FORMAT_YUV444:
      return "YUV444";
    case WL_SHM_FORMAT_YVU444:
      return "YVU444";
    case WL_SHM_FORMAT_R8:
      return "R8";
    case WL_SHM_FORMAT_R16:
      return "R16";
    case WL_SHM_FORMAT_RG88:
      return "RG88";
    case WL_SHM_FORMAT_GR88:
      return "GR88";
    case WL_SHM_FORMAT_RG1616:
      return "RG1616";
    case WL_SHM_FORMAT_GR1616:
      return "GR1616";
    case WL_SHM_FORMAT_XRGB16161616F:
      return "XRGB16161616F";
    case WL_SHM_FORMAT_XBGR16161616F:
      return "XBGR16161616F";
    case WL_SHM_FORMAT_ARGB16161616F:
      return "ARGB16161616F";
    case WL_SHM_FORMAT_ABGR16161616F:
      return "ABGR16161616F";
    case WL_SHM_FORMAT_XYUV8888:
      return "XYUV8888";
    case WL_SHM_FORMAT_VUY888:
      return "VUY888";
    case WL_SHM_FORMAT_VUY101010:
      return "VUY101010";
    case WL_SHM_FORMAT_Y210:
      return "Y210";
    case WL_SHM_FORMAT_Y212:
      return "Y212";
    case WL_SHM_FORMAT_Y216:
      return "Y216";
    case WL_SHM_FORMAT_Y410:
      return "Y410";
    case WL_SHM_FORMAT_Y412:
      return "Y412";
    case WL_SHM_FORMAT_Y416:
      return "Y416";
    case WL_SHM_FORMAT_XVYU2101010:
      return "XVYU2101010";
    case WL_SHM_FORMAT_XVYU12_16161616:
      return "XVYU12_16161616";
    case WL_SHM_FORMAT_XVYU16161616:
      return "XVYU16161616";
    case WL_SHM_FORMAT_Y0L0:
      return "Y0L0";
    case WL_SHM_FORMAT_X0L0:
      return "X0L0";
    case WL_SHM_FORMAT_Y0L2:
      return "Y0L2";
    case WL_SHM_FORMAT_X0L2:
      return "X0L2";
    case WL_SHM_FORMAT_YUV420_8BIT:
      return "YUV420_8BIT";
    case WL_SHM_FORMAT_YUV420_10BIT:
      return "YUV420_10BIT";
    case WL_SHM_FORMAT_XRGB8888_A8:
      return "XRGB8888_A8";
    case WL_SHM_FORMAT_XBGR8888_A8:
      return "XBGR8888_A8";
    case WL_SHM_FORMAT_RGBX8888_A8:
      return "RGBX8888_A8";
    case WL_SHM_FORMAT_BGRX8888_A8:
      return "BGRX8888_A8";
    case WL_SHM_FORMAT_RGB888_A8:
      return "RGB888_A8";
    case WL_SHM_FORMAT_BGR888_A8:
      return "BGR888_A8";
    case WL_SHM_FORMAT_RGB565_A8:
      return "RGB565_A8";
    case WL_SHM_FORMAT_BGR565_A8:
      return "BGR565_A8";
    case WL_SHM_FORMAT_NV24:
      return "NV24";
    case WL_SHM_FORMAT_NV42:
      return "NV42";
    case WL_SHM_FORMAT_P210:
      return "P210";
    case WL_SHM_FORMAT_P010:
      return "P010";
    case WL_SHM_FORMAT_P012:
      return "P012";
    case WL_SHM_FORMAT_P016:
      return "P016";
#ifdef WL_SHM_FORMAT_AXBXGXRX106106106106
    case WL_SHM_FORMAT_AXBXGXRX106106106106:
      return "AXBXGXRX106106106106";
#endif
#ifdef WL_SHM_FORMAT_NV15
    case WL_SHM_FORMAT_NV15:
      return "NV15";
#endif
#ifdef WL_SHM_FORMAT_Q410
    case WL_SHM_FORMAT_Q410:
      return "Q410";
#endif
#ifdef WL_SHM_FORMAT_Q401
    case WL_SHM_FORMAT_Q401:
      return "Q401";
#endif
#ifdef WL_SHM_FORMAT_XRGB16161616
    case WL_SHM_FORMAT_XRGB16161616:
      return "XRGB16161616";
#endif
#ifdef WL_SHM_FORMAT_XBGR16161616
    case WL_SHM_FORMAT_XBGR16161616:
      return "XBGR16161616";
#endif
#ifdef WL_SHM_FORMAT_ARGB16161616
    case WL_SHM_FORMAT_ARGB16161616:
      return "ARGB16161616";
#endif
#ifdef WL_SHM_FORMAT_ABGR16161616
    case WL_SHM_FORMAT_ABGR16161616:
      return "ABGR16161616";
#endif
  }
  return "UNKNOWN";
}

void Registrar::handle_interface_compositor(Registrar* r,
                                            wl_registry* registry,
                                            uint32_t name,
                                            const char* interface,
                                            uint32_t version) {
  r->wl_compositor_ = static_cast<wl_compositor*>(
      wl_registry_bind(registry, name, &wl_compositor_interface,
                       std::min(kWlCompositorMinVersion, version)));
  LOG_DEBUG("{}: {}", interface, wl_compositor_get_version(r->wl_compositor_));
}

void Registrar::handle_interface_subcompositor(Registrar* r,
                                               wl_registry* registry,
                                               uint32_t name,
                                               const char* interface,
                                               uint32_t version) {
  r->wl_subcompositor_ = static_cast<wl_subcompositor*>(
      wl_registry_bind(registry, name, &wl_subcompositor_interface,
                       std::min(kWlSubcompositorMinVersion, version)));
  LOG_DEBUG("{}: {}", interface,
            wl_subcompositor_get_version(r->wl_subcompositor_));
}

void Registrar::handle_interface_shm(Registrar* r,
                                     wl_registry* registry,
                                     uint32_t name,
                                     const char* interface,
                                     uint32_t version) {
  r->wl_shm_ = static_cast<struct wl_shm*>(wl_registry_bind(
      registry, name, &wl_shm_interface, std::min(kWlShmMinVersion, version)));
  wl_shm_add_listener(r->wl_shm_, &shm_listener_, r);
  LOG_DEBUG("{}: {}", interface, wl_shm_get_version(r->wl_shm_));
}

void Registrar::handle_interface_seat(Registrar* r,
                                      struct wl_registry* registry,
                                      uint32_t name,
                                      const char* interface,
                                      uint32_t version) {
  auto wl_seat = static_cast<struct wl_seat*>(
      wl_registry_bind(registry, name, &wl_seat_interface,
                       std::min(kWlSeatMinVersion, version)));
  if (!r->seats_.count(wl_seat)) {
    r->seats_[wl_seat] = std::make_unique<Seat>(
        wl_seat, r->get_shm(), r->get_compositor(), r->disable_cursor_);
    LOG_DEBUG("{}: {}", interface, wl_seat_get_version(wl_seat));
  }
}

void Registrar::handle_interface_output(Registrar* r,
                                        struct wl_registry* registry,
                                        uint32_t name,
                                        const char* interface,
                                        uint32_t version) {
  auto wl_output = static_cast<struct wl_output*>(
      wl_registry_bind(registry, name, &wl_output_interface,
                       std::min(kWlOutputMinVersion, version)));
  if (!r->outputs_.count(wl_output)) {
    r->outputs_[wl_output] =
        std::make_unique<Output>(wl_output, r->zxdg_output_manager_v1_);
    LOG_DEBUG("{}: {}", interface, wl_output_get_version(wl_output));
  }
}

#if ENABLE_XDG_CLIENT

void Registrar::handle_interface_xdg_wm_base(Registrar* r,
                                             struct wl_registry* registry,
                                             uint32_t name,
                                             const char* interface,
                                             uint32_t version) {
  r->xdg_wm_base_ = static_cast<struct xdg_wm_base*>(
      wl_registry_bind(registry, name, &xdg_wm_base_interface,
                       std::min(kXdgWmBaseMinVersion, version)));
  LOG_DEBUG("{}: {}", interface, xdg_wm_base_get_version(r->xdg_wm_base_));
}

#endif

#if ENABLE_AGL_SHELL_CLIENT

void Registrar::handle_interface_agl_shell(Registrar* r,
                                           struct wl_registry* registry,
                                           uint32_t name,
                                           const char* interface,
                                           uint32_t version) {
  r->agl_shell_ = static_cast<struct agl_shell*>(
      wl_registry_bind(registry, name, &agl_shell_interface,
                       std::min(kAglShellMinVersion, version)));
  LOG_DEBUG("{}: {}", interface, agl_shell_get_version(r->agl_shell_));
}

#endif

#if ENABLE_IVI_SHELL_CLIENT
void Registrar::handle_interface_ivi_wm(Registrar* r,
                                        struct wl_registry* registry,
                                        uint32_t name,
                                        const char* interface,
                                        uint32_t version) {
  r->ivi_wm_.ivi_wm = static_cast<struct ivi_wm*>(wl_registry_bind(
      registry, name, &ivi_wm_interface, std::min(kIviWmMinVersion, version)));
  LOG_DEBUG("{}: {}", interface, ivi_wm_get_version(r->ivi_wm_.ivi_wm.value()));
}
#endif

#if HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1

void Registrar::handle_interface_zxdg_decoration(Registrar* r,
                                                 struct wl_registry* registry,
                                                 uint32_t name,
                                                 const char* interface,
                                                 uint32_t version) {
  r->zxdg_decoration_manager_v1_ =
      static_cast<struct zxdg_decoration_manager_v1*>(wl_registry_bind(
          registry, name, &zxdg_decoration_manager_v1_interface,
          std::min(kXdgDecorationManagerMinVersion, version)));
  LOG_DEBUG(
      "{}: {}", interface,
      zxdg_decoration_manager_v1_get_version(r->zxdg_decoration_manager_v1_));
}

void Registrar::handle_interface_zxdg_toplevel_decoration(
    Registrar* r,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  r->zxdg_toplevel_decoration_v1_ =
      static_cast<struct zxdg_toplevel_decoration_v1*>(wl_registry_bind(
          registry, name, &zxdg_toplevel_decoration_v1_interface,
          std::min(kXdgDecorationManagerMinVersion, version)));
  LOG_DEBUG(
      "{}: {}", interface,
      zxdg_toplevel_decoration_v1_get_version(r->zxdg_toplevel_decoration_v1_));
}

#endif

#if HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1

void Registrar::handle_interface_xdg_output_unstable_v1(
    Registrar* r,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  r->zxdg_output_manager_v1_ = static_cast<struct zxdg_output_manager_v1*>(
      wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface,
                       std::min(kXdgOutputManagerMinVersion, version)));
  LOG_DEBUG("{}: {}", interface,
            zxdg_output_manager_v1_get_version(r->zxdg_output_manager_v1_));
}

#endif

void Registrar::handle_interface_weston_capture_v1(Registrar* r,
                                                   struct wl_registry* registry,
                                                   uint32_t name,
                                                   const char* interface,
                                                   uint32_t version) {
  r->weston_capture_v1_ = static_cast<struct weston_capture_v1*>(
      wl_registry_bind(registry, name, &weston_capture_v1_interface,
                       std::min(kWestonCaptureV1MinVersion, version)));
  LOG_DEBUG("{}: {}", interface,
            weston_capture_v1_get_version(r->weston_capture_v1_));
}

#if HAS_WAYLAND_PROTOCOL_IDLE_INHIBIT_UNSTABLE_V1

void Registrar::handle_interface_zwp_idle_inhibit_manager_v1(
    Registrar* r,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  r->zwp_idle_inhibit_manager_v1_ =
      static_cast<struct zwp_idle_inhibit_manager_v1*>(wl_registry_bind(
          registry, name, &zwp_idle_inhibit_manager_v1_interface,
          std::min(kIdleInhibitManagerV1MinVersion, version)));
  LOG_DEBUG(
      "{}: {}", interface,
      zwp_idle_inhibit_manager_v1_get_version(r->zwp_idle_inhibit_manager_v1_));
}

#endif

#if HAS_WAYLAND_PROTOCOL_XDG_ACTIVATION_V1

void Registrar::handle_interface_xdg_activation_v1(Registrar* r,
                                                   struct wl_registry* registry,
                                                   uint32_t name,
                                                   const char* interface,
                                                   uint32_t version) {
  r->xdg_activation_v1_ = static_cast<struct xdg_activation_v1*>(
      wl_registry_bind(registry, name, &xdg_activation_v1_interface,
                       std::min(kXdgActivationV1MinVersion, version)));
  LOG_DEBUG("{}: {}", interface,
            xdg_activation_v1_get_version(r->xdg_activation_v1_));
}

#endif

void Registrar::handle_presentation_clock_id(
    void* data,
    struct wp_presentation* wp_presentation,
    uint32_t clk_id) {
  auto r = static_cast<Registrar*>(data);
  if (r->presentation_time_.wp_presentation != wp_presentation) {
    return;
  }
  r->presentation_time_.clk_id = static_cast<clockid_t>(clk_id);
}

void Registrar::handle_interface_presentation(Registrar* r,
                                              struct wl_registry* registry,
                                              uint32_t name,
                                              const char* interface,
                                              uint32_t version) {
  r->presentation_time_.wp_presentation = static_cast<struct wp_presentation*>(
      wl_registry_bind(registry, name, &wp_presentation_interface,
                       std::min(kPresentationTimeMinVersion, version)));
  wp_presentation_add_listener(r->presentation_time_.wp_presentation,
                               &presentation_listener_, r);
  LOG_DEBUG("{}: {}", interface,
            wp_presentation_get_version(r->presentation_time_.wp_presentation));
}

std::optional<Seat*> Registrar::get_seat(wl_seat* seat) const {
  if (seat) {
    auto it = seats_.find(seat);
    if (it != seats_.end()) {
      return it->second.get();
    }
  } else {
    /// If seat is nullptr, return first available
    for (auto const& it : seats_) {
      return it.second.get();
    }
  }
  return {};
}

#if HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1

void Registrar::handle_interface_tearing_control_manager(
    Registrar* r,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  r->wp_tearing_control_manager_ =
      static_cast<struct wp_tearing_control_manager_v1*>(wl_registry_bind(
          registry, name, &wp_tearing_control_manager_v1_interface,
          std::min(kTearingControlManagerMinVersion, version)));
  LOG_DEBUG("{}: {}", interface,
            wp_tearing_control_manager_v1_get_version(
                r->wp_tearing_control_manager_));
}

#endif

#if HAS_WAYLAND_PROTOCOL_VIEWPORTER

void Registrar::handle_interface_viewporter(Registrar* r,
                                            struct wl_registry* registry,
                                            uint32_t name,
                                            const char* interface,
                                            uint32_t version) {
  r->wp_viewporter_ = static_cast<struct wp_viewporter*>(
      wl_registry_bind(registry, name, &wp_viewporter_interface,
                       std::min(kViewporterMinVersion, version)));
  LOG_DEBUG("{}: {}", interface, wp_viewporter_get_version(r->wp_viewporter_));
}

#endif

#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1

void Registrar::handle_interface_fractional_scale_manager(
    Registrar* r,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  r->fractional_scale_manager_ =
      static_cast<struct wp_fractional_scale_manager_v1*>(wl_registry_bind(
          registry, name, &wp_fractional_scale_manager_v1_interface,
          std::min(kFractionalScaleManagerMinVersion, version)));
  LOG_DEBUG(
      "{}: {}", interface,
      wp_fractional_scale_manager_v1_get_version(r->fractional_scale_manager_));
}

#endif

#if ENABLE_DRM_LEASE_CLIENT
void Registrar::handle_interface_drm_lease_device_v1(
    Registrar* r,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  auto wp_drm_lease_device_v1 = static_cast<struct wp_drm_lease_device_v1*>(
      wl_registry_bind(registry, name, &wp_drm_lease_device_v1_interface,
                       std::min(kDrmLeaseDeviceV1MinVersion, version)));
  r->drm_lease_device_v1_ =
      std::make_unique<DrmLeaseDevice_v1>(wp_drm_lease_device_v1);
  LOG_DEBUG("{}: {}", interface,
            wp_drm_lease_device_v1_get_version(wp_drm_lease_device_v1));
}
#endif

#if HAS_WAYLAND_PROTOCOL_POINTER_GESTURES_UNSTABLE_V1

void Registrar::handle_interface_zwp_pointer_gestures_v1(
    Registrar* r,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  r->zwp_pointer_gestures_v1_ = static_cast<struct zwp_pointer_gestures_v1*>(
      wl_registry_bind(registry, name, &zwp_pointer_gestures_v1_interface,
                       std::min(kPointerGesturesV1MinVersion, version)));
  LOG_DEBUG("{}: {}", interface,
            zwp_pointer_gestures_v1_get_version(r->zwp_pointer_gestures_v1_));
}

#endif

#if HAS_WAYLAND_PROTOCOL_POINTER_CONSTRAINTS_UNSTABLE_V1

void Registrar::handle_interface_zwp_pointer_constraints_v1(
    Registrar* r,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  r->zwp_pointer_constraints_v1_ =
      static_cast<struct zwp_pointer_constraints_v1*>(wl_registry_bind(
          registry, name, &zwp_pointer_constraints_v1_interface,
          std::min(kPointerConstraintsV1MinVersion, version)));
  LOG_DEBUG(
      "{}: {}", interface,
      zwp_pointer_constraints_v1_get_version(r->zwp_pointer_constraints_v1_));
}

#endif

#if HAS_WAYLAND_PROTOCOL_RELATIVE_POINTER_UNSTABLE_V1

void Registrar::handle_interface_zwp_relative_pointer_manager_v1(
    Registrar* r,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  r->zwp_relative_pointer_manager_v1_ =
      static_cast<struct zwp_relative_pointer_manager_v1*>(wl_registry_bind(
          registry, name, &zwp_relative_pointer_manager_v1_interface,
          std::min(kRelativePointerManagerV1MinVersion, version)));
  LOG_DEBUG("{}: {}", interface,
            zwp_relative_pointer_manager_v1_get_version(
                r->zwp_relative_pointer_manager_v1_));
}

#endif

#if HAS_WAYLAND_PROTOCOL_PRIMARY_SELECTION_UNSTABLE_V1

void Registrar::handle_interface_zwp_primary_selection_device_manager_v1(
    Registrar* r,
    struct wl_registry* registry,
    uint32_t name,
    const char* interface,
    uint32_t version) {
  r->zwp_primary_selection_device_manager_v1_ =
      static_cast<struct zwp_primary_selection_device_manager_v1*>(
          wl_registry_bind(
              registry, name,
              &zwp_primary_selection_device_manager_v1_interface,
              std::min(kPrimarySelectionDeviceManagerV1MinVersion, version)));
  LOG_DEBUG("{}: {}", interface,
            zwp_primary_selection_device_manager_v1_get_version(
                r->zwp_primary_selection_device_manager_v1_));
}

#endif