
#include "cursor.h"

#include <cassert>
#include <wayland-client-protocol.h>

#include "logging.h"


Cursor::Cursor(struct wl_shm *shm, struct wl_compositor *compositor, int size) {
    assert(shm);
    wl_surface_ = wl_compositor_create_surface(compositor);
    theme_ = wl_cursor_theme_load(nullptr, size, shm);
    if (!theme_) {
        spdlog::error("unable to load default theme");
        return;
    }
}

Cursor::~Cursor() {
    SPDLOG_TRACE("++Cursor::~Cursor()");
    if (theme_) {
        wl_cursor_theme_destroy(theme_);
    }
    SPDLOG_TRACE("--Cursor::~Cursor()");
}

void Cursor::update_pointer(struct wl_pointer *pointer, uint32_t serial, const char *name) {
    auto cursor = wl_cursor_theme_get_cursor(theme_, name);
    if (!cursor) {
        spdlog::error("unable to load {}", name);
        return;
    }
    auto image = cursor->images[0];
    auto buffer = wl_cursor_image_get_buffer(image);
    if (!buffer) {
        return;
    }
    wl_pointer_set_cursor(pointer, serial,
                          wl_surface_,
                          static_cast<int32_t>(image->hotspot_x),
                          static_cast<int32_t>(image->hotspot_y));
    wl_surface_attach(wl_surface_, buffer, 0, 0);
    wl_surface_damage(wl_surface_, 0, 0,
                      static_cast<int32_t>(image->width), static_cast<int32_t>(image->height));
    wl_surface_commit(wl_surface_);
}
