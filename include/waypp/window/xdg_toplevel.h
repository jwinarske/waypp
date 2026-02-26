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

#include <cstdint>
#include <memory>

#include "waypp/window/egl.h"
#include "waypp/window_manager/xdg_window_manager.h"
#include "window.h"

#if ENABLE_CSD
#include "waypp/window/csd_frame.h"
#endif

class Output;

class Window;

class XdgWindowManager;

class XdgTopLevel : public Window {
 public:
  XdgTopLevel(const std::shared_ptr<WindowManager>& wm,
              const char* title,
              const char* app_id,
              int width,
              int height,
              int resize_margin,
              int buffer_count,
              uint32_t buffer_format,
              bool fullscreen,
              bool maximized,
              bool fullscreen_ratio,
              bool tearing,
              const std::function<void(void*, const uint32_t)>& frame_callback,
              Egl::config* egl_config,
              bool enable_csd = false);

  ~XdgTopLevel();

  [[nodiscard]] uint32_t get_version() const {
    return xdg_toplevel_get_version(xdg_toplevel_);
  }

  [[nodiscard]] const std::string& get_app_id() const { return app_id_; }

  void set_app_id(const char* app_id) const {
    xdg_toplevel_set_app_id(xdg_toplevel_, app_id);
  }

  [[nodiscard]] const std::string& get_title() const { return title_; }

  void set_title(const char* title) const {
    xdg_toplevel_set_title(xdg_toplevel_, title);
  }

  void set_fullscreen() const {
    xdg_toplevel_set_fullscreen(xdg_toplevel_, nullptr);
  }

  void set_maximize() const { xdg_toplevel_set_maximized(xdg_toplevel_); }

  void set_minimize() const { xdg_toplevel_set_minimized(xdg_toplevel_); }

  void set_min_size(const int width, const int height) const {
    xdg_toplevel_set_min_size(xdg_toplevel_, width, height);
  }

  void set_max_size(const int width, const int height) const {
    xdg_toplevel_set_max_size(xdg_toplevel_, width, height);
  }

  void resize(wl_seat* seat, uint32_t serial, uint32_t edges) const;

  void move(wl_seat* seat, uint32_t serial) const {
    xdg_toplevel_move(xdg_toplevel_, seat, serial);
  }

  /// Returns the underlying xdg_toplevel object.
  /// Used by CsdFrame to create the per-window decoration object via
  /// zxdg_decoration_manager_v1_get_toplevel_decoration().
  [[nodiscard]] xdg_toplevel* get_xdg_toplevel() const { return xdg_toplevel_; }

  void set_surface_damage(const int x,
                          const int y,
                          const int width,
                          const int height) const {
    wl_surface_damage(get_surface(), x, y, width, height);
  }

  [[nodiscard]] xdg_toplevel_resize_edge check_edge_resize(
      const std::pair<double, double>& xy) const;

  [[nodiscard]] bool is_resizing() const { return get_resizing(); }

#if ENABLE_CSD
  /// Returns the current decoration extents, or {0,0,0,0} when CSD is
  /// disabled or the compositor chose server-side decorations.
  [[nodiscard]] const CsdFrameExtents& csd_extents() const;
#endif

  // Disallow copy and assign.
  XdgTopLevel(const XdgTopLevel&) = delete;

  XdgTopLevel& operator=(const XdgTopLevel&) = delete;

 private:
  std::shared_ptr<WindowManager> wm_;
  xdg_surface* xdg_surface_{};
  xdg_toplevel* xdg_toplevel_{};

  std::string title_;
  std::string app_id_;

  bool prev_state_[13]{false};
  int resize_margin_;

  uint32_t configure_serial_{};
  volatile bool wait_for_configure_;

#if ENABLE_CSD
  std::unique_ptr<CsdFrame> csd_frame_;
#endif

  static void handle_xdg_surface_configure(void* data,
                                           xdg_surface* xdg_surface,
                                           uint32_t serial);

  static constexpr xdg_surface_listener xdg_surface_listener_ = {
      .configure = handle_xdg_surface_configure,
  };

  static void handle_xdg_toplevel_configure(void* data,
                                            xdg_toplevel* toplevel,
                                            int32_t width,
                                            int32_t height,
                                            wl_array* states);

  static void handle_xdg_toplevel_close(void* data, xdg_toplevel* xdg_toplevel);

#if XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION

  static void handle_xdg_toplevel_configure_bounds(
      void* data,
      struct xdg_toplevel* xdg_toplevel,
      int32_t width,
      int32_t height);

#endif
#if XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION

  static void handle_xdg_toplevel_wm_capabilities(
      void* data,
      struct xdg_toplevel* xdg_toplevel,
      struct wl_array* capabilities);

#endif

  static constexpr xdg_toplevel_listener xdg_toplevel_listener_ = {
      .configure = handle_xdg_toplevel_configure,
      .close = handle_xdg_toplevel_close
#if XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION
      ,
      .configure_bounds = handle_xdg_toplevel_configure_bounds
#endif
#if XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION
      ,
      .wm_capabilities = handle_xdg_toplevel_wm_capabilities
#endif
  };
};
