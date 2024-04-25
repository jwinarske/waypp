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

#include "registrar.h"

#include "output.h"
#include "logging.h"

Registrar::Registrar(struct wl_display *wl_display) : wl_display_(wl_display),
                                                      wl_registry_(wl_display_get_registry(wl_display)) {
    SPDLOG_TRACE("++Registrar::Registrar()");
    wl_registry_add_listener(wl_registry_, &listener_,
                             this);
    wl_display_roundtrip(wl_display_);
    SPDLOG_TRACE("--Registrar::Registrar()");
}

Registrar::~Registrar() {
    SPDLOG_TRACE("++Registrar::~Registrar()");

    for (auto &it: output_.outputs) {
        it.second.reset();
        wl_output_destroy(it.first);
    }

    for (auto &it: seat_.seats) {
        it.second.reset();
        wl_seat_destroy(it.first);
    }

    if (shm_.wl_shm) {
        wl_shm_destroy(shm_.wl_shm);
        shm_.formats.clear();
    }

    if (compositor_.wl_compositor) {
        wl_compositor_destroy(compositor_.wl_compositor);
    }

    if (sub_compositor_.wl_subcompositor) {
        wl_subcompositor_destroy(sub_compositor_.wl_subcompositor);
    }

    if (xdg_wm_base_.xdg_wm_base.has_value()) {
        xdg_wm_base_destroy(xdg_wm_base_.xdg_wm_base.value());
    }

    if (agl_shell_.agl_shell.has_value()) {
        agl_shell_destroy(agl_shell_.agl_shell.value());
    }

    if (xdg_decoration_manager_.zxdg_toplevel_decoration_v1.has_value()) {
#if defined(WAYLAND_PROTOCOL_HAS_XDG_DECORATION)
        zxdg_toplevel_decoration_v1_destroy(xdg_decoration_manager_.zxdg_toplevel_decoration_v1.value());
#endif
    }

    if (xdg_decoration_manager_.zxdg_decoration_manager_v1.has_value()) {
#if defined(WAYLAND_PROTOCOL_HAS_XDG_DECORATION)
        zxdg_decoration_manager_v1_destroy(xdg_decoration_manager_.zxdg_decoration_manager_v1.value());
#endif
    }

    if (tearing_manager_.wp_tearing_control_manager.has_value()) {
#if defined(WAYLAND_PROTOCOL_HAS_TEARING_CONTROL)
        wp_tearing_control_manager_v1_destroy(tearing_manager_.wp_tearing_control_manager.value());
#endif
    }

    if (viewporter_.wp_viewporter.has_value()) {
#if defined(WAYLAND_PROTOCOL_HAS_VIEWPORTER)
        wp_viewporter_destroy(viewporter_.wp_viewporter.value());
#endif
    }

    if (fractional_scale_manager_.fractional_scale_manager.has_value()) {
#if defined(WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE)
        wp_fractional_scale_manager_v1_destroy(fractional_scale_manager_.fractional_scale_manager.value());
#endif
    }

    if (wl_registry_) {
        wl_registry_destroy(wl_registry_);
    }
    SPDLOG_TRACE("--Registrar::~Registrar()");
}

void Registrar::registry_handle_global(void *data,
                                       struct wl_registry *registry,
                                       uint32_t name,
                                       const char *interface,
                                       uint32_t version) {
    SPDLOG_TRACE("++Registrar::registry_handle_global()\t\n\t{}: {}", interface, version);
    auto r = static_cast<Registrar *>(data);

    std::map<std::string, std::function<void()>> interface_to_process_fn = {
            {wl_compositor_interface.name, [&]() {
                r->compositor_.wl_compositor = static_cast<struct wl_compositor *>(
                        wl_registry_bind(registry, name, &wl_compositor_interface,
                                         std::min(static_cast<uint32_t>(r->compositor_.min_version), version)));
                SPDLOG_DEBUG("{}: {}", wl_compositor_interface.name,
                             wl_compositor_get_version(r->compositor_.wl_compositor));
            }},
            {wl_subcompositor_interface.name, [&]() {
                r->sub_compositor_.wl_subcompositor = static_cast<struct wl_subcompositor *>(
                        wl_registry_bind(registry, name, &wl_subcompositor_interface,
                                         std::min(static_cast<uint32_t>(r->sub_compositor_.min_version), version)));
                SPDLOG_DEBUG("{}: {}", wl_subcompositor_interface.name,
                             wl_subcompositor_get_version(r->sub_compositor_.wl_subcompositor));
            }},
            {wl_shm_interface.name, [&]() {
                r->shm_.wl_shm = static_cast<struct wl_shm *>(
                        wl_registry_bind(registry, name, &wl_shm_interface,
                                         std::min(static_cast<uint32_t>(r->shm_.min_version), version)));
                wl_shm_add_listener(r->shm_.wl_shm, &shm_listener_, r);
                SPDLOG_DEBUG("{}: {}", wl_shm_interface.name, wl_shm_get_version(r->shm_.wl_shm));
            }},
            {wl_seat_interface.name, [&]() {
                auto wl_seat = static_cast<struct wl_seat *>(
                        wl_registry_bind(registry, name, &wl_seat_interface,
                                         std::min(static_cast<uint32_t>(r->seat_.min_version), version)));
                r->seat_.seats[wl_seat] = std::make_unique<Seat>(wl_seat);
                SPDLOG_DEBUG("{}: {}", wl_seat_interface.name, wl_seat_get_version(wl_seat));
            }},
            {wl_output_interface.name, [&]() {
                auto wl_output = static_cast<struct wl_output *>(
                        wl_registry_bind(registry, name, &wl_output_interface,
                                         std::min(static_cast<uint32_t>(r->output_.min_version), version)));
                r->output_.outputs[wl_output] = std::make_unique<Output>(wl_output);
                SPDLOG_DEBUG("{}: {}", wl_output_interface.name, wl_output_get_version(wl_output));
            }},
            {xdg_wm_base_interface.name, [&]() {
                r->xdg_wm_base_.xdg_wm_base = static_cast<struct xdg_wm_base *>(
                        wl_registry_bind(registry, name, &xdg_wm_base_interface,
                                         std::min(static_cast<uint32_t>(r->xdg_wm_base_.min_version), version)));
                SPDLOG_DEBUG("{}: {}", xdg_wm_base_interface.name,
                             xdg_wm_base_get_version(r->xdg_wm_base_.xdg_wm_base.value()));
            }},
            {agl_shell_interface.name, [&]() {
                r->agl_shell_.agl_shell = static_cast<struct agl_shell *>(
                        wl_registry_bind(registry, name, &agl_shell_interface,
                                         std::min(static_cast<uint32_t>(r->agl_shell_.min_version), version)));
                SPDLOG_DEBUG("{}: {}", agl_shell_interface.name,
                             agl_shell_get_version(r->agl_shell_.agl_shell.value()));
            }},
            {ivi_wm_interface.name, [&]() {
                r->ivi_wm_.ivi_wm = static_cast<struct ivi_wm *>(
                        wl_registry_bind(registry, name, &ivi_wm_interface,
                                         std::min(static_cast<uint32_t>(r->ivi_wm_.min_version), version)));
                SPDLOG_DEBUG("{}: {}", ivi_wm_interface.name,
                             ivi_wm_get_version(r->ivi_wm_.ivi_wm.value()));
            }},
#if defined(WAYLAND_PROTOCOL_HAS_XDG_DECORATION)
            {
                    zxdg_decoration_manager_v1_interface.name, [&]() -> void {
                r->xdg_decoration_manager_.zxdg_decoration_manager_v1 = static_cast<struct zxdg_decoration_manager_v1 *>(
                        wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface,
                                         std::min(static_cast<uint32_t>(r->xdg_decoration_manager_.min_version),
                                                  version)));
                SPDLOG_DEBUG("{}: {}", zxdg_decoration_manager_v1_interface.name,
                             zxdg_decoration_manager_v1_get_version(
                                     r->xdg_decoration_manager_.zxdg_decoration_manager_v1.value()));
            }},
            {
                    zxdg_toplevel_decoration_v1_interface.name, [&]() -> void {
                r->xdg_decoration_manager_.zxdg_toplevel_decoration_v1 = static_cast<struct zxdg_toplevel_decoration_v1 *>(
                        wl_registry_bind(registry, name, &zxdg_toplevel_decoration_v1_interface,
                                         std::min(static_cast<uint32_t>(r->xdg_decoration_manager_.min_version),
                                                  version)));
                SPDLOG_DEBUG("{}: {}", zxdg_toplevel_decoration_v1_interface.name,
                             zxdg_toplevel_decoration_v1_get_version(
                                     r->xdg_decoration_manager_.zxdg_toplevel_decoration_v1.value()));
            }},
#endif
#if defined(WAYLAND_PROTOCOL_HAS_PRESENTATION_TIME)
            {
                    wp_presentation_interface.name, [&]() {
                r->presentation_time_.wp_presentation_time = static_cast<struct wp_presentation *>(
                        wl_registry_bind(registry, name, &wp_presentation_interface,
                                         std::min(static_cast<uint32_t>(r->presentation_time_.min_version),
                                                  version)));
                SPDLOG_DEBUG("{}: {}", wp_presentation_interface.name,
                             wp_presentation_get_version(r->presentation_time_.wp_presentation_time.value()));
            }},
#endif
#if defined(WAYLAND_PROTOCOL_HAS_TEARING_CONTROL)
            {
                    wp_tearing_control_manager_v1_interface.name, [&]() {
                auto r = static_cast<Registrar *>(data);
                r->tearing_manager_.wp_tearing_control_manager = static_cast<struct wp_tearing_control_manager_v1 *>(
                        wl_registry_bind(registry, name, &wp_tearing_control_manager_v1_interface,
                                         std::min(static_cast<uint32_t>(r->tearing_manager_.min_version),
                                                  version)));
                SPDLOG_DEBUG("{}: {}", wp_tearing_control_manager_v1_interface.name,
                             wp_tearing_control_manager_v1_get_version(
                                     r->tearing_manager_.wp_tearing_control_manager.value()));
            }},
#endif
#if defined(WAYLAND_PROTOCOL_HAS_VIEWPORTER)
            {
                    wp_viewporter_interface.name, [&]() {
                auto r = static_cast<Registrar *>(data);
                r->viewporter_.wp_viewporter = static_cast<struct wp_viewporter *>(
                        wl_registry_bind(registry, name, &wp_viewporter_interface,
                                         std::min(static_cast<uint32_t>(r->viewporter_.min_version), version)));
                SPDLOG_DEBUG("{}: {}", wp_viewporter_interface.name,
                             wp_viewporter_get_version(r->viewporter_.wp_viewporter.value()));
            }},
#endif
#if defined(WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE)
            {
                    wp_fractional_scale_manager_v1_interface.name, [&]() {
                auto r = static_cast<Registrar *>(data);
                r->fractional_scale_manager_.fractional_scale_manager = static_cast<struct wp_fractional_scale_manager_v1 *>(
                        wl_registry_bind(registry, name, &wp_fractional_scale_manager_v1_interface,
                                         std::min(static_cast<uint32_t>(r->fractional_scale_manager_.min_version),
                                                  version)));
                SPDLOG_DEBUG("{}: {}", wp_fractional_scale_manager_v1_interface.name,
                             wp_fractional_scale_manager_v1_get_version(
                                     r->fractional_scale_manager_.fractional_scale_manager.value()));
            }},
#endif
    };

    auto found = interface_to_process_fn.find(interface);
    if (found != interface_to_process_fn.end()) {
        found->second();  // Execute the process function for the found interface.
    }
    SPDLOG_TRACE("--Registrar::registry_handle_global()");
}

void Registrar::registry_handle_global_remove(void *data,
                                              struct wl_registry *reg,
                                              uint32_t id) {
    SPDLOG_TRACE("++Registrar::registry_handle_global_remove()");
    auto obj = static_cast<Registrar *>(data);
    std::scoped_lock<std::mutex> lock(obj->registrar_global_remove_mutex_);
    if (obj->registrar_remove_.find(id) != obj->registrar_remove_.end()) {
        auto p = obj->registrar_remove_[id];
        p.first(p.second, reg, id);
    }
    SPDLOG_TRACE("--Registrar::registry_handle_global_remove()");
}

enum wl_output_transform Registrar::get_output_buffer_transform(struct wl_output *wl_output) {
    for (auto &output: output_.outputs) {
        if (wl_output == output.first) {
            output.second->print();
            return output.second->get_transform();
            break;
        }
    }
    return WL_OUTPUT_TRANSFORM_NORMAL;
}

int32_t Registrar::get_output_buffer_scale(struct wl_output *wl_output) {
    int32_t scale = 1;
    for (auto &output: output_.outputs) {
        if (wl_output == output.first) {
            scale = output.second->get_scale_factor();
            break;
        }
    }
    return scale;
}

void Registrar::shm_format(void *data, struct wl_shm *wl_shm, uint32_t format) {
    SPDLOG_TRACE("++Registrar::shm_format()");
    auto obj = static_cast<Registrar *>(data);
    if (obj->shm_.wl_shm != wl_shm) {
        return;
    }
    obj->shm_.formats.push_back(format);
    SPDLOG_TRACE("--Registrar::shm_format()");
}

const char *Registrar::shm_format_to_text(enum wl_shm_format format) {
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
        case WL_SHM_FORMAT_AXBXGXRX106106106106:
            return "AXBXGXRX106106106106";
        case WL_SHM_FORMAT_NV15:
            return "NV15";
        case WL_SHM_FORMAT_Q410:
            return "Q410";
        case WL_SHM_FORMAT_Q401:
            return "Q401";
        case WL_SHM_FORMAT_XRGB16161616:
            return "XRGB16161616";
        case WL_SHM_FORMAT_XBGR16161616:
            return "XBGR16161616";
        case WL_SHM_FORMAT_ARGB16161616:
            return "ARGB16161616";
        case WL_SHM_FORMAT_ABGR16161616:
            return "ABGR16161616";
    }
    return "UNKNOWN";
}
