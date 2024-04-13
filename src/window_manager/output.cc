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

#include "output.h"

#include <wayland-client-protocol.h>

#include "logging.h"

/**
 * @class Output
 * @brief The Output class represents a Wayland output.
 *
 * This class manages the state and listeners for a Wayland output, providing
 * access to the output's properties such as geometry and mode. It also handles
 * the events emitted by the output.
 */
Output::Output(struct wl_output *wl_output) : wl_output_(wl_output) {
    SPDLOG_TRACE("++Output::Output()");
    wl_output_add_listener(wl_output_, &listener_, this);
    SPDLOG_TRACE("--Output::Output()");
}

Output::~Output() = default;

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
    SPDLOG_TRACE("++Output::handle_geometry()");
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
    SPDLOG_TRACE("--Output::handle_geometry()");
}

void Output::handle_mode(void *data,
                         struct wl_output *wl_output,
                         uint32_t flags,
                         int width,
                         int height,
                         int refresh) {
    SPDLOG_TRACE("++Output::handle_mode()");
    auto obj = static_cast<Output *>(data);
    if (obj->wl_output_ != wl_output) {
        return;
    }
    obj->output_.mode = {
            .flags = flags,
            .width = width,
            .height = height,
            .refresh = refresh
    };
    SPDLOG_TRACE("--Output::handle_mode()");
}

void Output::handle_scale(void *data,
                          struct wl_output *wl_output,
                          int32_t factor) {
    SPDLOG_TRACE("++Output::handle_scale()");
    auto obj = static_cast<Output *>(data);
    if (obj->wl_output_ != wl_output) {
        return;
    }
    obj->output_.factor = factor;
    SPDLOG_TRACE("++Output::handle_scale()");
}

void Output::handle_done(void *data,
                         struct wl_output *wl_output) {
    SPDLOG_TRACE("++Output::handle_done()");
    auto obj = static_cast<Output *>(data);
    if (wl_output != obj->wl_output_) {
        return;
    }

    auto output = obj->output_;

    output.done = true;
    SPDLOG_TRACE("--Output::handle_done()");
}

void Output::handle_name(void *data,
                         struct wl_output *wl_output,
                         const char *name) {
    SPDLOG_TRACE("++Output::handle_name()");
    auto obj = static_cast<Output *>(data);
    if (obj->wl_output_ != wl_output) {
        return;
    }
    obj->output_.name = name;
    SPDLOG_TRACE("--Output::handle_name()");
}

void Output::handle_desc(void *data,
                         struct wl_output *wl_output,
                         const char *desc) {
    SPDLOG_TRACE("++Output::handle_desc()");
    auto obj = static_cast<Output *>(data);
    if (obj->wl_output_ != wl_output) {
        return;
    }
    obj->output_.description = desc;
    SPDLOG_TRACE("--Output::handle_desc()");
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
    spdlog::info("Output");
#if defined(WL_OUTPUT_NAME_SINCE_VERSION)
    spdlog::info("\tName: {}", output_.name);
#endif
#if defined(WL_OUTPUT_DESCRIPTION_SINCE_VERSION)
    spdlog::info("\tDescription: {}", output_.description);
#endif
    spdlog::info("\tMode");
    spdlog::info("\t\tSize: {}x{}", output_.mode.width, output_.mode.height);
    spdlog::info("\t\tRefresh: {}", output_.mode.refresh);
    spdlog::info("\t\tFlags: ");
    if ((output_.mode.flags & WL_OUTPUT_MODE_CURRENT) == WL_OUTPUT_MODE_CURRENT) {
        spdlog::info("\t\t\tWL_OUTPUT_MODE_CURRENT");
    }
    if ((output_.mode.flags & WL_OUTPUT_MODE_PREFERRED) == WL_OUTPUT_MODE_PREFERRED) {
        spdlog::info("\t\t\tWL_OUTPUT_MODE_PREFERRED");
    }
    spdlog::info("\tGeometry");
    spdlog::info("\t\tMake: {}", output_.geometry.make);
    spdlog::info("\t\tModel: {}", output_.geometry.model);
    spdlog::info("\t\tPhysical: {}x{}", output_.geometry.physical_width, output_.geometry.physical_height);
    spdlog::info("\t\tSubpixel: {}", output_.geometry.subpixel);
    spdlog::info("\t\tTransform: {}", transform_to_string(output_.geometry.transform));
    spdlog::info("\t\tx: {}, y: {}", output_.geometry.x, output_.geometry.y);
#if defined(WL_OUTPUT_SCALE_SINCE_VERSION)
    spdlog::info("\tScaling factor: {}", output_.factor);
#endif
}
