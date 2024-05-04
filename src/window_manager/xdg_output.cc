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

#include "xdg_output.h"

#include "logging.h"

XdgOutput::XdgOutput(struct zxdg_output_manager_v1 *zxdg_output_manager_v1, struct wl_output *wl_output)
        : zxdg_output_manager_v1_(
        zxdg_output_manager_v1) {
    zxdg_output_v1_ = zxdg_output_manager_v1_get_xdg_output(zxdg_output_manager_v1, wl_output);
    zxdg_output_v1_add_listener(zxdg_output_v1_, &listener_, this);
}

XdgOutput::~XdgOutput() {
    if (zxdg_output_v1_) {
        zxdg_output_v1_destroy(zxdg_output_v1_);
    }
}

void XdgOutput::handle_logical_position(void *data, struct zxdg_output_v1 *zxdg_output_v1, int32_t x, int32_t y) {
    auto obj = static_cast<XdgOutput *>(data);
    if (obj->zxdg_output_v1_ != zxdg_output_v1) {
        return;
    }
    spdlog::debug("XdgOutput::handle_logical_position: x: {}, y: {}", x, y);

    obj->output_.logical_position = {
            .x = x,
            .y = y,
    };
}

void XdgOutput::handle_logical_size(void *data, struct zxdg_output_v1 *zxdg_output_v1, int32_t width, int32_t height) {
    auto obj = static_cast<XdgOutput *>(data);
    if (obj->zxdg_output_v1_ != zxdg_output_v1) {
        return;
    }
    spdlog::debug("XdgOutput::handle_logical_size: width: {}, height: {}", width, height);

    obj->output_.logical_size = {
            .width = width,
            .height = height,
    };
}

void XdgOutput::handle_done(void *data, struct zxdg_output_v1 *zxdg_output_v1) {
    auto obj = static_cast<XdgOutput *>(data);
    if (obj->zxdg_output_v1_ != zxdg_output_v1) {
        return;
    }
    spdlog::debug("XdgOutput::handle_done");
    obj->output_.done = true;
}

void XdgOutput::handle_name(void *data, struct zxdg_output_v1 *zxdg_output_v1, const char *name) {
    auto obj = static_cast<XdgOutput *>(data);
    if (obj->zxdg_output_v1_ != zxdg_output_v1) {
        return;
    }
    spdlog::debug("XdgOutput::handle_name: {}", name);
    obj->output_.name = name;
}

void XdgOutput::handle_description(void *data, struct zxdg_output_v1 *zxdg_output_v1, const char *description) {
    auto obj = static_cast<XdgOutput *>(data);
    if (obj->zxdg_output_v1_ != zxdg_output_v1) {
        return;
    }
    spdlog::debug("XdgOutput::handle_description: {}", description);
    obj->output_.description = description;
}

void XdgOutput::print() const {
    spdlog::info("XDG Output");
    spdlog::info("\tDone: {}", output_.done);
    spdlog::info("\tName: {}", output_.name);
    spdlog::info("\tDescription: {}", output_.description);
    spdlog::info("\tlogical_position");
    spdlog::info("\t\tx: {}", output_.logical_position.x);
    spdlog::info("\t\ty: {}", output_.logical_position.y);
    spdlog::info("\tlogical_size");
    spdlog::info("\t\tx: {}", output_.logical_size.width);
    spdlog::info("\t\ty: {}", output_.logical_size.height);
}
