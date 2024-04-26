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

class AglShell;

class WindowManager;

class Registrar {
public:
    typedef void (*RegistrarGlobalCallback)(
            void *data,
            struct wl_registry *registry,
            uint32_t name,
            const char *interface,
            uint32_t version);

    typedef void (*RegistrarGlobalRemoveCallback)(
            void *data,
            struct wl_registry *registry,
            uint32_t id);

    struct RegistrarCallback {
        const char *interface;
        RegistrarGlobalCallback global_callback;
        RegistrarGlobalRemoveCallback global_remove_callback;
    };

    explicit Registrar(struct wl_display *wl_display,
                       unsigned long ext_interface_count = 0,
                       const RegistrarCallback *ext_interface_data = nullptr);

    ~Registrar();

    // Returns if shared memory has a specific format.
    [[nodiscard]] std::optional<bool> shm_has_format(enum wl_shm_format format) const {
        if (!shm_.wl_shm.has_value()) {
            return {};
        }
        if (std::find(shm_.formats.begin(), shm_.formats.end(), format) != shm_.formats.end()) {
            return true;
        }
        return false;
    }

    // Returns text representation of wl_shm_format
    static const char *shm_format_to_text(enum wl_shm_format format);

    // Returns the registry.
    [[nodiscard]] struct wl_registry *get_registry() const { return wl_registry_; }

    // Returns the compositor.
    [[nodiscard]] struct wl_compositor *get_compositor() const { return compositor_.wl_compositor; }

    // Returns the subcompositor if available.
    [[nodiscard]] std::optional<struct wl_subcompositor *>
    get_subcompositor() const { return sub_compositor_.wl_subcompositor; }

    // Returns the shm if it exists.
    [[nodiscard]] std::optional<struct wl_shm *> get_shm() const { return shm_.wl_shm; }

    // Returns the xdg surface manager base if it exists.
    [[nodiscard]] std::optional<struct xdg_wm_base *> get_xdg_wm_base() const { return xdg_wm_base_.xdg_wm_base; }

    // Returns the AGL Shell if it exists.
    [[nodiscard]] std::optional<struct agl_shell *> get_agl_shell() const { return agl_shell_.agl_shell; }

    // Returns the ivi surface manager if it exists.
    [[nodiscard]] std::optional<struct ivi_wm *> get_ivi_wm() const { return ivi_wm_.ivi_wm; }

    [[nodiscard]] std::optional<struct wp_presentation *>
    get_presentation_time() const { return presentation_time_.wp_presentation_time; }

    [[nodiscard]] std::optional<struct wp_tearing_control_manager_v1 *>
    get_tearing_control_manager() const { return tearing_manager_.wp_tearing_control_manager; }

    [[nodiscard]] std::optional<struct wp_viewporter *>
    get_viewporter() const { return viewporter_.wp_viewporter; }

    [[nodiscard]] std::optional<struct wp_fractional_scale_manager_v1 *>
    get_fractional_scale_manager() const { return fractional_scale_manager_.fractional_scale_manager; }

    [[nodiscard]] const std::map<struct wl_output *, std::unique_ptr<Output>> &
    get_outputs() const { return output_.outputs; }

    enum wl_output_transform get_output_buffer_transform(struct wl_output *wl_output);

    int32_t get_output_buffer_scale(struct wl_output *wl_output);

    // Disallow copy and assign.
    Registrar(const Registrar &) = delete;

    Registrar &operator=(const Registrar &) = delete;

private:
    friend AglShell;
    friend WindowManager;

    std::unique_ptr<std::map<std::string, RegistrarGlobalCallback>> registrar_global_;
    std::unique_ptr<std::map<uint32_t, RegistrarGlobalRemoveCallback>> registrar_global_remove_;

    struct wl_display *wl_display_;
    struct wl_registry *wl_registry_;

    struct {
        uint32_t min_version = kWlSeatMinVersion;
        std::map<struct wl_seat *, std::unique_ptr<Seat>> seats;
    } seat_;

    struct {
        uint32_t min_version = kWlShmMinVersion;
        std::optional<struct wl_shm *> wl_shm;
        std::vector<uint32_t> formats;
    } shm_;

    struct {
        uint32_t min_version = kWlCompositorMinVersion;
        struct wl_compositor *wl_compositor{};
    } compositor_;

    struct {
        uint32_t min_version = kWlSubcompositorMinVersion;
        std::optional<struct wl_subcompositor *> wl_subcompositor;
    } sub_compositor_;

    struct {
        uint32_t min_version = kXdgWmBaseMinVersion;
        std::optional<struct xdg_wm_base *> xdg_wm_base;
    } xdg_wm_base_;

    struct {
        uint32_t min_version = kAglShellMinVersion;
        std::optional<struct agl_shell *> agl_shell;
    } agl_shell_;

    struct {
        uint32_t min_version = kIviWmMinVersion;
        std::optional<struct ivi_wm *> ivi_wm;
    } ivi_wm_;

    struct {
        uint32_t min_version = kXdgDecorationManagerMinVersion;
        std::optional<struct zxdg_decoration_manager_v1 *> zxdg_decoration_manager_v1;
        std::optional<struct zxdg_toplevel_decoration_v1 *> zxdg_toplevel_decoration_v1;
    } xdg_decoration_manager_;

    struct {
        uint32_t min_version = kPresentationTimeMinVersion;
        std::optional<struct wp_presentation *> wp_presentation_time;
    } presentation_time_;

    struct {
        uint32_t min_version = kTearingControlManagerMinVersion;
        std::optional<struct wp_tearing_control_manager_v1 *> wp_tearing_control_manager;
    } tearing_manager_;

    struct {
        uint32_t min_version = kViewporterMinVersion;
        std::optional<struct wp_viewporter *> wp_viewporter;
    } viewporter_;

    struct {
        uint32_t min_version = kFractionalScaleManagerMinVersion;
        std::optional<struct wp_fractional_scale_manager_v1 *> fractional_scale_manager;
    } fractional_scale_manager_;

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
            .global_remove = registry_handle_global_remove,
    };

    // Handles shm format events.
    static void shm_format(void *data,
                           struct wl_shm *wl_shm,
                           uint32_t format);

    static constexpr wl_shm_listener shm_listener_ = {
            .format = shm_format,
    };

    static void handle_interface_compositor(void *data,
                                            struct wl_registry *registry,
                                            uint32_t name,
                                            const char *interface,
                                            uint32_t version);

    static void handle_interface_subcompositor(void *data,
                                               struct wl_registry *registry,
                                               uint32_t name,
                                               const char *interface,
                                               uint32_t version);

    static void handle_interface_shm(void *data,
                                     struct wl_registry *registry,
                                     uint32_t name,
                                     const char *interface,
                                     uint32_t version);

    static void handle_interface_seat(void *data,
                                      struct wl_registry *registry,
                                      uint32_t name,
                                      const char *interface,
                                      uint32_t version);

    static void handle_interface_output(void *data,
                                        struct wl_registry *registry,
                                        uint32_t name,
                                        const char *interface,
                                        uint32_t version);

    static void handle_interface_xdg_wm_base(void *data,
                                             struct wl_registry *registry,
                                             uint32_t name,
                                             const char *interface,
                                             uint32_t version);

    static void handle_interface_agl_shell(void *data,
                                           struct wl_registry *registry,
                                           uint32_t name,
                                           const char *interface,
                                           uint32_t version);

    static void handle_interface_ivi_wm(void *data,
                                        struct wl_registry *registry,
                                        uint32_t name,
                                        const char *interface,
                                        uint32_t version);

#if defined(HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1)

    static void handle_interface_zxdg_decoration(void *data,
                                                 struct wl_registry *registry,
                                                 uint32_t name,
                                                 const char *interface,
                                                 uint32_t version);

    static void handle_interface_zxdg_toplevel_decoration(void *data,
                                                          struct wl_registry *registry,
                                                          uint32_t name,
                                                          const char *interface,
                                                          uint32_t version);

#endif

#if defined(HAS_WAYLAND_PROTOCOL_PRESENTATION_TIME)

    static void handle_interface_presentation(void *data,
                                              struct wl_registry *registry,
                                              uint32_t name,
                                              const char *interface,
                                              uint32_t version);

#endif

#if defined(HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1)

    static void handle_interface_tearing_control_manager(void *data,
                                                         struct wl_registry *registry,
                                                         uint32_t name,
                                                         const char *interface,
                                                         uint32_t version);

#endif

#if defined(HAS_WAYLAND_PROTOCOL_VIEWPORTER)

    static void handle_interface_viewporter(void *data,
                                            struct wl_registry *registry,
                                            uint32_t name,
                                            const char *interface,
                                            uint32_t version);

#endif

#if defined(HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1)

    static void handle_interface_fractional_scale_manager(void *data,
                                                          struct wl_registry *registry,
                                                          uint32_t name,
                                                          const char *interface,
                                                          uint32_t version);

#endif

protected:
    struct {
        uint32_t min_version = kWlOutputMinVersion;
        std::map<struct wl_output *, std::unique_ptr<Output>> outputs;
    } output_;
};
