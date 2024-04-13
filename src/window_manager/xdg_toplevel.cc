#include "xdg_toplevel.h"

#include "logging.h"

// workaround for Wayland macro not compiling in C++
#define WL_ARRAY_FOR_EACH(pos, array, type)                             \
  for (pos = (type)(array)->data;                                       \
       (const char*)pos < ((const char*)(array)->data + (array)->size); \
       (pos)++)

XdgTopLevel::XdgTopLevel(WindowManager *wm,
                         struct wl_compositor *wl_compositor,
                         const std::optional<struct wp_viewporter *> &viewporter,
                         const std::optional<struct wp_fractional_scale_manager_v1 *> &fractional_scale_manager,
                         const std::optional<struct wp_tearing_control_manager_v1 *> &tearing_control_manager,
                         const std::map<struct wl_output *, std::unique_ptr<Output>> &outputs,
                         const char *name, int width, int height, wl_output_transform buffer_transform,
                         bool fullscreen, bool maximized, bool fullscreen_ratio, bool tearing,
                         const std::function<void(void *, const uint32_t)> &draw_frame_callback,
                         const int32_t *context_attribs, size_t context_attribs_size,
                         const int32_t *config_attribs, size_t config_attribs_size,
                         int buffer_bpp, int swap_interval) : Window(
        wm, wl_compositor, viewporter, fractional_scale_manager,
        tearing_control_manager, outputs, name, draw_frame_callback, width, height, buffer_transform, fullscreen,
        maximized, fullscreen_ratio, tearing, context_attribs, context_attribs_size, config_attribs,
        config_attribs_size, buffer_bpp, swap_interval), wm_(wm) {
    auto xwm = reinterpret_cast<XdgWindowManager *>(wm_);
    auto xdg_wm_base = xwm->get_xdg_wm_base();
    if (!xdg_wm_base.has_value()) {
        spdlog::critical("xdg_wm_base is not available");
        exit(EXIT_FAILURE);
    }
    SPDLOG_DEBUG("XDG Toplevel Surface: {}", fmt::ptr(get_surface()));
    xdg_surface_ = xdg_wm_base_get_xdg_surface(xdg_wm_base.value(), get_surface());
    xdg_surface_add_listener(xdg_surface_, &xdg_surface_listener_, this);

    xdg_toplevel_ = xdg_surface_get_toplevel(xdg_surface_);
    xdg_toplevel_add_listener(xdg_toplevel_, &xdg_toplevel_listener_, this);

    xdg_toplevel_set_title(xdg_toplevel_, title_.c_str());
    xdg_toplevel_set_app_id(xdg_toplevel_, app_id_.c_str());

    if (fullscreen) {
        xdg_toplevel_set_fullscreen(xdg_toplevel_, nullptr);
    } else if (maximized) {
        xdg_toplevel_set_maximized(xdg_toplevel_);
    }

    wait_for_configure_ = true;
    wl_surface_commit(get_surface());

    // this makes the start-up from the beginning with the correct dimensions
    // like starting as maximized/fullscreen, rather than starting up as floating
    // width, height then performing a resize
    while (wait_for_configure_) {
        wl_display_dispatch(wm_->get_display());

        // wait until xdg_surface::configure ACKs the new dimensions
        if (wait_for_configure_)
            continue;
    }
}

XdgTopLevel::~XdgTopLevel() {
    if (xdg_toplevel_)
        xdg_toplevel_destroy(xdg_toplevel_);

    if (xdg_surface_)
        xdg_surface_destroy(xdg_surface_);
}

void XdgTopLevel::resize(int /* width */, int /* height */) {
}

/**
 * @brief Handles the configure event for xdg_surface.
 *
 * This function is a member function of the XdgWm class. It is called when the xdg_surface
 * sends a configure event. It acknowledges the configure request by calling xdg_surface_ack_configure().
 * It also sets the wait_for_configure_ variable to false.
 *
 * @param data A pointer to the XdgWm instance.
 * @param xdg_surface A pointer to the xdg_surface instance.
 * @param serial The serial number of the configure event.
 */
void XdgTopLevel::handle_xdg_surface_configure(
        void *data,
        struct xdg_surface *xdg_surface,
        uint32_t serial) {
    auto *w = static_cast<XdgTopLevel *>(data);
    if (w->xdg_surface_ != xdg_surface) {
        return;
    }
    xdg_surface_ack_configure(xdg_surface, serial);
    w->wait_for_configure_ = false;
}

/**
 * @brief Handles the configure event for a toplevel surface.
 *
 * This function is called when the configure event is received for a toplevel surface.
 * It updates the internal state of the XdgWm object based on the configuration properties
 * received from the compositor.
 *
 * @param data The user data passed to the callback.
 * @param toplevel The toplevel surface that triggered the event.
 * @param width The width of the surface.
 * @param height The height of the surface.
 * @param states An array of states associated with the surface.
 */
void XdgTopLevel::handle_toplevel_configure(
        void *data,
        struct xdg_toplevel *toplevel,
        int32_t width,
        int32_t height,
        struct wl_array *states) {

    if (width == 0 && height == 0) {
        return;
    }

    auto *w = static_cast<XdgTopLevel *>(data);
    if (w->xdg_toplevel_ != toplevel) {
        return;
    }

    w->fullscreen_ = false;
    w->maximized_ = false;
    w->resize_ = false;
    w->activated_ = false;

    const uint32_t *state;
    WL_ARRAY_FOR_EACH(state, states, const uint32_t*) {
        switch (*state) {
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                SPDLOG_DEBUG("XDG_TOPLEVEL_STATE_FULLSCREEN");
                w->fullscreen_ = true;
                break;
            case XDG_TOPLEVEL_STATE_MAXIMIZED:
                SPDLOG_DEBUG("XDG_TOPLEVEL_STATE_MAXIMIZED");
                w->maximized_ = true;
                break;
            case XDG_TOPLEVEL_STATE_RESIZING:
                SPDLOG_DEBUG("XDG_TOPLEVEL_STATE_RESIZING");
                w->resize_ = true;
                break;
            case XDG_TOPLEVEL_STATE_ACTIVATED:
                SPDLOG_DEBUG("XDG_TOPLEVEL_STATE_ACTIVATED");
                w->activated_ = true;
                break;
        }
    }

    if (width > 0 && height > 0) {
        if (!w->fullscreen_ && !w->maximized_) {
            w->window_size_.width = width;
            w->window_size_.height = height;
        }
        w->logical_size_.width = width;
        w->logical_size_.height = height;
    } else if (!w->fullscreen_ && !w->maximized_) {
        w->logical_size_.width = w->window_size_.width;
        w->logical_size_.height = w->window_size_.height;
    }

    w->set_needs_buffer_geometry_update();

    SPDLOG_DEBUG("width: {}", width);
    SPDLOG_DEBUG("height: {}", height);
}

/**
 * @brief Handles the close event of a toplevel surface.
 *
 * This function is a callback that is called when the user requests to close
 * the toplevel surface. It sets the `running_` member variable to false, which
 * will cause the main event loop to exit.
 *
 * @param data The user data associated with the XdgWm instance.
 * @param xdg_toplevel The xdg_toplevel object that received the close request.
 */
void XdgTopLevel::handle_toplevel_close(
        void *data,
        struct xdg_toplevel *xdg_toplevel) {
    SPDLOG_DEBUG("XdgWm::handle_toplevel_close");

    auto *w = static_cast<XdgTopLevel *>(data);
    if (w->xdg_toplevel_ != xdg_toplevel) {
        return;
    }
    w->running_ = false;
}

/**
 *
 * @param data
 * @param xdg_toplevel
 * @param width
 * @param height
 */
#if defined(XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION)

void XdgTopLevel::handle_configure_bounds(void *data,
                                          struct xdg_toplevel *xdg_toplevel,
                                          int32_t width,
                                          int32_t height) {
    auto *w = static_cast<XdgTopLevel *>(data);
    if (w->xdg_toplevel_ != xdg_toplevel) {
        return;
    }
    SPDLOG_DEBUG("Configure Bounds: {}x{}", width, height);
    w->window_size_.width = width;
    w->window_size_.height = height;
}

#endif

/**
 *
 * @param data
 * @param xdg_toplevel
 * @param capabilities
 */
#if defined(XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)

void XdgTopLevel::handle_wm_capabilities(void *data,
                                         struct xdg_toplevel *xdg_toplevel,
                                         struct wl_array *capabilities) {
    auto *w = static_cast<XdgTopLevel *>(data);
    if (w->xdg_toplevel_ != xdg_toplevel) {
        return;
    }
    SPDLOG_DEBUG("WM Capabilities:");
    const uint32_t *cap;
    WL_ARRAY_FOR_EACH(cap, capabilities, const uint32_t*) {
        switch (*cap) {
            case XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU:
                SPDLOG_DEBUG("\tXDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU");
                break;
            case XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE:
                SPDLOG_DEBUG("\tXDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE");
                break;
            case XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN:
                SPDLOG_DEBUG("\tXDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN");
                break;
            case XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE:
                SPDLOG_DEBUG("\tXDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE");
                break;
        }
    }
}

#endif
