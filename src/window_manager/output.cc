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

#include "waypp/window_manager/output.h"

#include <wayland-client-protocol.h>

#include "logging/logging.h"

/**
 * @class Output
 * @brief The Output class represents a Wayland output.
 *
 * This class manages the state and listeners for a Wayland output, providing
 * access to the output's properties such as geometry and mode. It also handles
 * the events emitted by the output.
 */
Output::Output(struct wl_output *wl_output,
               struct zxdg_output_manager_v1 *zxdg_output_manager_v1)
        : wl_output_(wl_output), zxdg_output_manager_v1_(zxdg_output_manager_v1) {
    DLOG_TRACE("++Output::Output()");
    wl_output_add_listener(wl_output_, &listener_, this);
    DLOG_TRACE("--Output::Output()");
}

Output::~Output() {
    if (wl_output_) {
        DLOG_TRACE("[Output] wl_output_destroy(wl_output_)");
        wl_output_destroy(wl_output_);
    }
    if (xdg_output_) {
        xdg_output_.reset();
    }
}

void Output::handle_geometry(void *data,
                             struct wl_output *wl_output,
                             int x,
                             int y,
                             int physical_width,
                             int physical_height,
                             int subpixel,
                             const char *make,
                             const char *model,
                             int transform) {
    DLOG_TRACE("++Output::handle_geometry()");
    auto obj = static_cast<Output *>(data);
    if (obj->wl_output_ != wl_output) {
        return;
    }
    obj->output_.geometry = {
            .x = x,
            .y = y,
            .physical_width = physical_width,
            .physical_height = physical_height,
            .subpixel = subpixel,
            .make = make,
            .model = model,
            .transform = static_cast<enum wl_output_transform>(transform),
    };
    if (obj->zxdg_output_manager_v1_ && !obj->xdg_output_) {
        obj->xdg_output_ = std::make_unique<XdgOutput>(obj->zxdg_output_manager_v1_,
                                                       obj->wl_output_);
    }
    DLOG_TRACE("--Output::handle_geometry()");
}

void Output::handle_mode(void *data,
                         struct wl_output *wl_output,
                         uint32_t flags,
                         int width,
                         int height,
                         int refresh) {
    DLOG_TRACE("++Output::handle_mode()");
    auto obj = static_cast<Output *>(data);
    if (obj->wl_output_ != wl_output) {
        return;
    }
    obj->output_.mode = {
            .flags = flags, .width = width, .height = height, .refresh = refresh};
    DLOG_TRACE("--Output::handle_mode()");
}

void Output::handle_scale(void *data,
                          struct wl_output *wl_output,
                          int32_t factor) {
    LOG_TRACE("++Output::handle_scale()");
    auto obj = static_cast<Output *>(data);
    if (obj->wl_output_ != wl_output) {
        return;
    }
    obj->output_.factor = factor;
    LOG_TRACE("++Output::handle_scale()");
}

void Output::handle_done(void *data, struct wl_output *wl_output) {
    LOG_TRACE("++Output::handle_done()");
    auto obj = static_cast<Output *>(data);
    if (wl_output != obj->wl_output_) {
        return;
    }

    auto output = obj->output_;

    output.done = true;
    LOG_TRACE("--Output::handle_done()");
}

void Output::handle_name(void *data,
                         struct wl_output *wl_output,
                         const char *name) {
    LOG_TRACE("++Output::handle_name()");
    auto obj = static_cast<Output *>(data);
    if (obj->wl_output_ != wl_output) {
        return;
    }
    obj->output_.name = name;
    LOG_TRACE("--Output::handle_name()");
}

void Output::handle_desc(void *data,
                         struct wl_output *wl_output,
                         const char *desc) {
    LOG_TRACE("++Output::handle_desc()");
    auto obj = static_cast<Output *>(data);
    if (obj->wl_output_ != wl_output) {
        return;
    }
    obj->output_.description = desc;
    LOG_TRACE("--Output::handle_desc()");
}

std::string Output::transform_to_string(enum wl_output_transform transform) {
    switch (transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
            return "WL_OUTPUT_TRANSFORM_NORMAL";
        case WL_OUTPUT_TRANSFORM_90:
            return "WL_OUTPUT_TRANSFORM_90";
        case WL_OUTPUT_TRANSFORM_180:
            return "WL_OUTPUT_TRANSFORM_180";
        case WL_OUTPUT_TRANSFORM_270:
            return "WL_OUTPUT_TRANSFORM_270";
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            return "WL_OUTPUT_TRANSFORM_FLIPPED";
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            return "WL_OUTPUT_TRANSFORM_FLIPPED_90";
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            return "WL_OUTPUT_TRANSFORM_FLIPPED_180";
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            return "WL_OUTPUT_TRANSFORM_FLIPPED_270";
    }
    return {};
}

void Output::print() {
    LOG_INFO("Output");
    LOG_INFO("\tDone: {}", output_.done);
#if WL_OUTPUT_NAME_SINCE_VERSION
    LOG_INFO("\tName: {}", output_.name);
#endif
#if WL_OUTPUT_DESCRIPTION_SINCE_VERSION
    LOG_INFO("\tDescription: {}", output_.description);
#endif
    LOG_INFO("\tMode");
    LOG_INFO("\t\tSize: {}x{}", output_.mode.width, output_.mode.height);
    LOG_INFO("\t\tRefresh: {}", output_.mode.refresh);
    LOG_INFO("\t\tFlags: ");
    if ((output_.mode.flags & WL_OUTPUT_MODE_CURRENT) == WL_OUTPUT_MODE_CURRENT) {
        LOG_INFO("\t\t\tWL_OUTPUT_MODE_CURRENT");
    }
    if ((output_.mode.flags & WL_OUTPUT_MODE_PREFERRED) ==
        WL_OUTPUT_MODE_PREFERRED) {
        LOG_INFO("\t\t\tWL_OUTPUT_MODE_PREFERRED");
    }
    LOG_INFO("\tGeometry");
    LOG_INFO("\t\tMake: {}", output_.geometry.make);
    LOG_INFO("\t\tModel: {}", output_.geometry.model);
    LOG_INFO("\t\tPhysical: {}x{}", output_.geometry.physical_width,
                 output_.geometry.physical_height);
    LOG_INFO("\t\tSubpixel: {}", output_.geometry.subpixel);
    LOG_INFO("\t\tTransform: {}",
                 transform_to_string(output_.geometry.transform));
    LOG_INFO("\t\tx: {}, y: {}", output_.geometry.x, output_.geometry.y);
#if WL_OUTPUT_SCALE_SINCE_VERSION
    LOG_INFO("\tScaling factor: {}", output_.factor);
#endif
}
