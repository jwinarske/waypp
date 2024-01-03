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

#include <cassert>

/**
 * @class Output
 * @brief The Output class represents a Wayland output.
 *
 * This class manages the state and listeners for a Wayland output, providing
 * access to the output's properties such as geometry and mode. It also handles
 * the events emitted by the output.
 */
Output::Output(struct wl_output *output, uint32_t version) : version_(
        version), wl_output_(output) {
    wl_output_add_listener(wl_output_, &listener_, this);
}

/**
 * @class Output
 * @brief The Output class represents a Wayland output.
 *
 * The Output class provides methods to manage Wayland outputs, such as releasing and destroying the output.
 */
Output::~Output() {
    wl_output_release(wl_output_);
    wl_output_destroy(wl_output_);
}

/**
 * @brief Handle the geometry event of the wl_output interface.
 *
 * This function is called when the wl_output interface emits the
 * geometry event, indicating changes in the output's position, size,
 * and physical properties.
 *
 * @param data              Pointer to the Output object.
 * @param wl_output         The wl_output object.
 * @param x                 The x coordinate of the output's position.
 * @param y                 The y coordinate of the output's position.
 * @param physical_width    The physical width of the output in millimeters.
 * @param physical_height   The physical height of the output in millimeters.
 * @param subpixel          The subpixel arrangement of the output.
 * @param make              The make of the output device.
 * @param model             The model of the output device.
 * @param transform         The transform applied to the output.
 */
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
    const auto obj = static_cast<Output *>(data);
    assert(obj->wl_output_ == wl_output);
    obj->output_ = {
            .geometry = {
                    .x = x,
                    .y = y,
                    .physical_width = physical_width,
                    .physical_height = physical_height,
                    .subpixel = subpixel,
                    .make = make,
                    .model = model,
                    .transform = transform
            },
            .mode = {},
            .done{},
            .name{},
            .description{},
    };
}

/**
* @brief This function is responsible for handling the mode of the output.
*
* The handle_mode function is called when the mode of the output is updated. It sets the mode of the output
* structure to the provided values.
*
* @param data A pointer to the instance of the Output class.
* @param wl_output A pointer to the wl_output structure.
* @param flags The flags of the mode.
* @param width The width of the mode.
* @param height The height of the mode.
* @param refresh The refresh rate of the mode.
*/
void Output::handle_mode(void *data,
                         struct wl_output *wl_output,
                         uint32_t flags,
                         int width,
                         int height,
                         int refresh) {
    const auto obj = static_cast<Output *>(data);
    assert(obj->wl_output_ == wl_output);
    obj->output_.mode = {
            .flags = flags,
            .width = width,
            .height = height,
            .refresh = refresh,
    };
}

/**
 * @brief Handle the completion of an output event.
 *
 * This function is a callback that is invoked when an output event is completed.
 *
 * @param data A pointer to the associated Output object.
 * @param wl_output The Wayland output object.
 */
void Output::handle_done(void *data, struct wl_output *wl_output) {
    const auto obj = static_cast<Output *>(data);
    assert(obj->wl_output_ == wl_output);
    obj->output_.done = true;
}

/**
 * @brief Callback function for handling output scale change.
 *
 * This function is called when the scale of the output is changed.
 * It updates the scale value of the Output object.
 *
 * @param data The user data associated with the Output object.
 * @param wl_output The wl_output object associated with the event.
 * @param scale The new scale value.
 */
void Output::handle_scale(void *data,
                          struct wl_output *wl_output,
                          int scale) {
    const auto obj = static_cast<Output *>(data);
    assert(obj->wl_output_ == wl_output);
    obj->output_.scale = scale;
}

/**
 * @brief Handle the name event from the wl_output interface.
 *
 * This function is called when the name property of the output is updated.
 *
 * @param data A pointer to the Output object.
 * @param wl_output A pointer to the wl_output object.
 * @param name The new name of the output.
 */
void Output::handle_name(void *data,
                         struct wl_output *wl_output,
                         const char *name) {
    const auto obj = static_cast<Output *>(data);
    assert(obj->wl_output_ == wl_output);
    obj->output_.name = name;
}

/**
 * @brief Handles the description of an output.
 *
 * This function is invoked when a description is received for a specific output. It updates the description in the
 * `output_` member of the `Output` object.
 *
 * @param data      A pointer to the `Output` object.
 * @param wl_output A pointer to the `wl_output` object.
 * @param description The description of the output.
 */
void Output::handle_description(void *data,
                                struct wl_output *wl_output,
                                const char *description) {
    const auto obj = static_cast<Output *>(data);
    assert(obj->wl_output_ == wl_output);
    obj->output_.description = description;
}

const struct wl_output_listener Output::listener_ = {
        .geometry = handle_geometry,
        .mode = handle_mode,
        .done = handle_done,
        .scale = handle_scale,
        .name = handle_name,
        .description = handle_description,
};
