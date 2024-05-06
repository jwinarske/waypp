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

#pragma once

#include <wayland-client.h>

#include <cstdint>
#include <algorithm>
#include <map>
#include <mutex>
#include <vector>

#include "config.h"
#include "output.h"
#include "wayland-protocols.h"
#include "seat/seat.h"

#if ENABLE_DRM_LEASE_CLIENT
#include "drm_lease_device_v1.h"
#endif

class Registrar {
public:
    typedef void (*RegistrarGlobalCallback)(
            Registrar *registrar,
            struct wl_registry *registry,
            uint32_t name,
            const char *interface,
            uint32_t version);

    typedef void (*RegistrarGlobalRemoveCallback)(
            Registrar *registrar,
            struct wl_registry *registry,
            uint32_t id);

    struct RegistrarCallback {
        const char *interface;
        RegistrarGlobalCallback global_callback;
        RegistrarGlobalRemoveCallback global_remove_callback;
    };

    explicit Registrar(struct wl_display *wl_display,
                       unsigned long ext_interface_count = 0,
                       const RegistrarCallback *ext_interface_data = nullptr,
                       bool disable_cursor = false);

    ~Registrar();

    // Returns if shared memory has a specific format.
    [[nodiscard]] std::optional<bool> shm_has_format(enum wl_shm_format format) const {
        if (!wl_shm_) {
            return {};
        }
        if (std::find(wl_shm_formats_.begin(), wl_shm_formats_.end(), format) != wl_shm_formats_.end()) {
            return true;
        }
        return false;
    }

    // Returns text representation of wl_shm_format
    static const char *shm_format_to_text(enum wl_shm_format format);

    // Returns the registry.
    [[nodiscard]] struct wl_registry *get_registry() const { return wl_registry_; }

    // Returns the compositor.
    [[nodiscard]] struct wl_compositor *get_compositor() const { return wl_compositor_; }

    // Returns the subcompositor if available.
    [[nodiscard]] struct wl_subcompositor *get_subcompositor() const { return wl_subcompositor_; }

    // Returns the shm if it exists.
    [[nodiscard]] struct wl_shm *get_shm() const { return wl_shm_; }

    // Returns the xdg surface manager base if it exists.
    [[nodiscard]] struct xdg_wm_base *get_xdg_wm_base() const { return xdg_wm_base_; }

    // Returns the AGL Shell if it exists.
    [[nodiscard]] struct agl_shell *get_agl_shell() const { return agl_shell_; }

    // Returns the ivi surface manager if it exists.
    [[nodiscard]] struct ivi_wm *get_ivi_wm() const { return ivi_wm_; }

    [[nodiscard]] struct wp_presentation *
    get_presentation_time_wp_presentation() const { return presentation_time_.wp_presentation; }

    [[nodiscard]] clockid_t get_presentation_time_clk_id() const { return presentation_time_.clk_id; }

    [[nodiscard]] struct wp_tearing_control_manager_v1 *
    get_tearing_control_manager() const { return wp_tearing_control_manager_; }

    [[nodiscard]] struct wp_viewporter *get_viewporter() const { return wp_viewporter_; }

    [[nodiscard]] struct wp_fractional_scale_manager_v1 *
    get_fractional_scale_manager() const { return fractional_scale_manager_; }

    [[nodiscard]] struct zxdg_output_manager_v1 *get_xdg_output_manager() const { return zxdg_output_manager_v1_; }

    [[nodiscard]] struct weston_capture_v1 *get_weston_capture_v1() const { return weston_capture_v1_; }

    [[nodiscard]] const std::map<struct wl_output *, std::unique_ptr<Output>> &get_outputs() const { return outputs_; }

    enum wl_output_transform get_output_buffer_transform(struct wl_output *wl_output);

    int32_t get_output_buffer_scale(struct wl_output *wl_output);


    std::optional<Seat *> get_seat(wl_seat *seat = nullptr) const;

    // Disallow copy and assign.
    Registrar(const Registrar &) = delete;

    Registrar &operator=(const Registrar &) = delete;

private:
    std::unique_ptr<std::map<std::string, RegistrarGlobalCallback>> registrar_global_;
    std::unique_ptr<std::map<uint32_t, RegistrarGlobalRemoveCallback>> registrar_global_remove_;

    bool disable_cursor_;

    struct wl_display *wl_display_;
    struct wl_registry *wl_registry_;

    std::map<struct wl_seat *, std::unique_ptr<Seat>> seats_;

    std::vector<uint32_t> wl_shm_formats_{};

    struct wl_compositor *wl_compositor_{};
    struct wl_shm *wl_shm_{};
    struct wl_subcompositor *wl_subcompositor_{};
    struct xdg_wm_base *xdg_wm_base_{};
    struct agl_shell *agl_shell_{};
    struct ivi_wm *ivi_wm_{};

#if ENABLE_DRM_LEASE_CLIENT
    std::unique_ptr<DrmLeaseDevice_v1> drm_lease_device_v1_;
#endif

    struct zxdg_decoration_manager_v1 *zxdg_decoration_manager_v1_{};
    struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1_{};

    struct {
        struct wp_presentation *wp_presentation;
        clockid_t clk_id;
    } presentation_time_{};

    struct wp_tearing_control_manager_v1 *wp_tearing_control_manager_{};
    struct wp_viewporter *wp_viewporter_{};
    struct wp_fractional_scale_manager_v1 *fractional_scale_manager_{};
    struct zxdg_output_manager_v1 *zxdg_output_manager_v1_{};
    struct weston_capture_v1 *weston_capture_v1_{};

    std::mutex registrar_global_mutex_;
    std::mutex registrar_global_remove_mutex_;

    // Handles global registry events.
    static void registry_handle_global(void *data,
                                       struct wl_registry *registry,
                                       uint32_t name,
                                       const char *interface,
                                       uint32_t version);

    // Handles global registry removal events.
    static void registry_handle_global_remove(void *data,
                                              struct wl_registry *reg,
                                              uint32_t id);

    static constexpr const wl_registry_listener listener_ = {
            .global = registry_handle_global,
            .global_remove = nullptr,
    };

    // Handles shm format events.
    static void shm_format(void *data,
                           struct wl_shm *wl_shm,
                           uint32_t format);

    static constexpr wl_shm_listener shm_listener_ = {
            .format = shm_format,
    };

    static void handle_interface_compositor(Registrar *r,
                                            struct wl_registry *registry,
                                            uint32_t name,
                                            const char *interface,
                                            uint32_t version);

    static void handle_interface_subcompositor(Registrar *r,
                                               struct wl_registry *registry,
                                               uint32_t name,
                                               const char *interface,
                                               uint32_t version);

    static void handle_interface_shm(Registrar *r,
                                     struct wl_registry *registry,
                                     uint32_t name,
                                     const char *interface,
                                     uint32_t version);

    static void handle_interface_seat(Registrar *r,
                                      struct wl_registry *registry,
                                      uint32_t name,
                                      const char *interface,
                                      uint32_t version);

    static void handle_interface_output(Registrar *r,
                                        struct wl_registry *registry,
                                        uint32_t name,
                                        const char *interface,
                                        uint32_t version);

#if ENABLE_XDG_CLIENT

    static void handle_interface_xdg_wm_base(Registrar *r,
                                             struct wl_registry *registry,
                                             uint32_t name,
                                             const char *interface,
                                             uint32_t version);

#endif

#if ENABLE_AGL_SHELL_CLIENT

    static void handle_interface_agl_shell(Registrar *r,
                                           struct wl_registry *registry,
                                           uint32_t name,
                                           const char *interface,
                                           uint32_t version);

#endif

#if ENABLE_IVI_SHELL_CLIENT
    static void handle_interface_ivi_wm(Registrar *r,
                                        struct wl_registry *registry,
                                        uint32_t name,
                                        const char *interface,
                                        uint32_t version);
#endif

#if ENABLE_DRM_LEASE_CLIENT

    static void handle_interface_drm_lease_device_v1(Registrar *r,
                                                     struct wl_registry *registry,
                                                     uint32_t name,
                                                     const char *interface,
                                                     uint32_t version);

#endif

#if HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1

    static void handle_interface_zxdg_decoration(Registrar *r,
                                                 struct wl_registry *registry,
                                                 uint32_t name,
                                                 const char *interface,
                                                 uint32_t version);

    static void handle_interface_zxdg_toplevel_decoration(Registrar *r,
                                                          struct wl_registry *registry,
                                                          uint32_t name,
                                                          const char *interface,
                                                          uint32_t version);

#endif

    static void handle_presentation_clock_id(void *data, struct wp_presentation *presentation, uint32_t clk_id);

    static constexpr struct wp_presentation_listener presentation_listener_ = {
            handle_presentation_clock_id
    };

    static void handle_interface_presentation(Registrar *r,
                                              struct wl_registry *registry,
                                              uint32_t name,
                                              const char *interface,
                                              uint32_t version);

#if HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1

    static void handle_interface_tearing_control_manager(Registrar *r,
                                                         struct wl_registry *registry,
                                                         uint32_t name,
                                                         const char *interface,
                                                         uint32_t version);

#endif

#if HAS_WAYLAND_PROTOCOL_VIEWPORTER

    static void handle_interface_viewporter(Registrar *r,
                                            struct wl_registry *registry,
                                            uint32_t name,
                                            const char *interface,
                                            uint32_t version);

#endif

#if HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1

    static void handle_interface_fractional_scale_manager(Registrar *r,
                                                          struct wl_registry *registry,
                                                          uint32_t name,
                                                          const char *interface,
                                                          uint32_t version);

#endif

#if HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1

    static void handle_interface_xdg_output_unstable_v1(Registrar *r,
                                                        struct wl_registry *registry,
                                                        uint32_t name,
                                                        const char *interface,
                                                        uint32_t version);

#endif

    static void handle_interface_weston_capture_v1(Registrar *r,
                                                   struct wl_registry *registry,
                                                   uint32_t name,
                                                   const char *interface,
                                                   uint32_t version);

protected:
    std::map<struct wl_output *, std::unique_ptr<Output>> outputs_;
};
