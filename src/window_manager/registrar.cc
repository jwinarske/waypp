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

void Registrar::shm_format(void *data, struct wl_shm *wl_shm, uint32_t format) {
    SPDLOG_TRACE("++Registrar::shm_format()");
    auto obj = static_cast<Registrar *>(data);
    if (obj->shm_.wl_shm != wl_shm) {
        return;
    }
    obj->shm_.formats.push_back(format);
    SPDLOG_TRACE("--Registrar::shm_format()");
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
