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

#ifndef SRC_OUTPUT_H_
#define SRC_OUTPUT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <wayland-client.h>


class Output {
public:
    struct geometry {
        int x;
        int y;
        int physical_width;
        int physical_height;
        int subpixel;
        const char *make;
        const char *model;
        int transform;
    };

    struct mode {
        uint32_t flags;
        int width;
        int height;
        int refresh;
    };

    Output(struct wl_output *output, uint32_t version);

    ~Output();

    [[nodiscard]] const struct geometry &get_geometry() const { return output_.geometry; }

    [[nodiscard]] const struct mode &get_mode() const { return output_.mode; }

    [[nodiscard]] uint32_t get_version() const { return version_; }

private:
    struct {
        struct geometry geometry;
        struct mode mode;
        bool done;
        std::optional<int> scale;
        std::string name;
        std::string description;
    } output_;

    uint32_t version_;
    struct wl_output *wl_output_;

    static void handle_geometry(void *data,
                                struct wl_output *wl_output,
                                int x,
                                int y,
                                int physical_width,
                                int physical_height,
                                int subpixel,
                                const char *make,
                                const char *model,
                                int transform);

    static void handle_mode(void *data,
                            struct wl_output *wl_output,
                            uint32_t flags,
                            int width,
                            int height,
                            int refresh);

    static void handle_done(void *data, struct wl_output *wl_output);

    static void handle_scale(void *data,
                             struct wl_output *wl_output,
                             int scale);

    static void handle_name(void *data,
                            struct wl_output *wl_output,
                            const char *name);

    static void handle_description(void *data,
                                   struct wl_output *wl_output,
                                   const char *description);

    static const struct wl_output_listener listener_;
};

#endif //SRC_OUTPUT_H_