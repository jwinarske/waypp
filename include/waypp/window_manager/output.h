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
#include <optional>
#include <string>
#include <unordered_map>

#include <wayland-client.h>

#include "xdg_output.h"

class XdgOutput;

class Output {
public:
    explicit Output(wl_output *output,
                    zxdg_output_manager_v1 *zxdg_output_manager_v1);

    ~Output();

    [[nodiscard]] int32_t get_scale_factor() const { return output_.factor; }

    [[nodiscard]] wl_output_transform get_transform() const {
        return output_.geometry.transform;
    }

    [[nodiscard]] const std::string &get_name() const { return output_.name; }

    void print();

    static std::string transform_to_string(wl_output_transform transform);

    const XdgOutput *get_xdg_output() const { return xdg_output_.get(); }

    // Disallow copy and assign.
    Output(const Output &) = delete;

    Output &operator=(const Output &) = delete;

private:
    wl_output *wl_output_;
    zxdg_output_manager_v1 *zxdg_output_manager_v1_;
    std::unique_ptr<XdgOutput> xdg_output_;

    struct {
        struct {
            int x;
            int y;
            int physical_width;
            int physical_height;
            int subpixel;
            std::string make;
            std::string model;
            wl_output_transform transform;
        } geometry;

        struct {
            uint32_t flags;
            int width;
            int height;
            int refresh;
        } mode;

        int32_t factor;
        std::string name;
        std::string description;
        bool done;

    } output_;

    static void handle_geometry(void *data,
                                wl_output *wl_output,
                                int x,
                                int y,
                                int physical_width,
                                int physical_height,
                                int subpixel,
                                const char *make,
                                const char *model,
                                int transform);

    static void handle_mode(void *data,
                            wl_output *wl_output,
                            uint32_t flags,
                            int width,
                            int height,
                            int refresh);

    static void handle_done(void *data, wl_output *wl_output);

    static void handle_scale(void *data,
                             wl_output *wl_output,
                             int32_t factor);

    static void handle_name(void *data,
                            wl_output *wl_output,
                            const char *name);

    static void handle_desc(void *data,
                            wl_output *wl_output,
                            const char *desc);

    static constexpr wl_output_listener listener_ = {handle_geometry,
                                                            handle_mode,
                                                            handle_done
#if WL_OUTPUT_SCALE_SINCE_VERSION
            ,
                                                            handle_scale
#endif
#if WL_OUTPUT_NAME_SINCE_VERSION
            ,
                                                            handle_name
#endif
#if WL_OUTPUT_DESCRIPTION_SINCE_VERSION
            ,
                                                            handle_desc
#endif
    };
};
