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

#include "wayland-protocols.h"

#include "output.h"

class XdgOutput {
public:
    XdgOutput(struct zxdg_output_manager_v1 *zxdg_output_manager_v1,
              struct wl_output *wl_output);

    ~XdgOutput();

    [[nodiscard]] int get_logical_position_x() const {
        return output_.logical_position.x;
    }

    [[nodiscard]] int get_logical_position_y() const {
        return output_.logical_position.y;
    }

    [[nodiscard]] int get_logical_size_width() const {
        return output_.logical_size.width;
    }

    [[nodiscard]] int get_logical_size_height() const {
        return output_.logical_size.height;
    }

    [[nodiscard]] const std::string &get_name() const { return output_.name; }

    [[nodiscard]] const std::string &get_description() const {
        return output_.description;
    }

    [[nodiscard]] bool is_origin() const {
        return (output_.logical_position.x == 0 && output_.logical_position.y == 0);
    }

    void print() const;

    // Disallow copy and assign.
    XdgOutput(const XdgOutput &) = delete;

    XdgOutput &operator=(const XdgOutput &) = delete;

private:
#if HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1
    struct zxdg_output_v1 *zxdg_output_v1_;
#endif

    struct {
        struct {
            int x;
            int y;
        } logical_position;

        struct {
            int width;
            int height;
        } logical_size;

        std::string name;
        std::string description;

        bool done;

    } output_;

    /**
     * position of the output within the global compositor space
     *
     * The position event describes the location of the wl_output
     * within the global compositor space.
     *
     * The logical_position event is sent after creating an xdg_output
     * (see xdg_output_manager.get_xdg_output) and whenever the
     * location of the output changes within the global compositor
     * space.
     * @param x x position within the global compositor space
     * @param y y position within the global compositor space
     */
    static void handle_logical_position(void *data,
                                        struct zxdg_output_v1 *zxdg_output_v1,
                                        int32_t x,
                                        int32_t y);

    /**
     * size of the output in the global compositor space
     *
     * The logical_size event describes the size of the output in the
     * global compositor space.
     *
     * Most regular Wayland clients should not pay attention to the
     * logical size and would rather rely on xdg_shell interfaces.
     *
     * Some clients such as Xwayland, however, need this to configure
     * their surfaces in the global compositor space as the compositor
     * may apply a different scale from what is advertised by the
     * output scaling property (to achieve fractional scaling, for
     * example).
     *
     * For example, for a wl_output mode 3840×2160 and a scale factor
     * 2:
     *
     * - A compositor not scaling the monitor viewport in its
     * compositing space will advertise a logical size of 3840×2160,
     *
     * - A compositor scaling the monitor viewport with scale factor 2
     * will advertise a logical size of 1920×1080,
     *
     * - A compositor scaling the monitor viewport using a fractional
     * scale of 1.5 will advertise a logical size of 2560×1440.
     *
     * For example, for a wl_output mode 1920×1080 and a 90 degree
     * rotation, the compositor will advertise a logical size of
     * 1080x1920.
     *
     * The logical_size event is sent after creating an xdg_output (see
     * xdg_output_manager.get_xdg_output) and whenever the logical size
     * of the output changes, either as a result of a change in the
     * applied scale or because of a change in the corresponding output
     * mode(see wl_output.mode) or transform (see wl_output.transform).
     * @param width width in global compositor space
     * @param height height in global compositor space
     */
    static void handle_logical_size(void *data,
                                    struct zxdg_output_v1 *zxdg_output_v1,
                                    int32_t width,
                                    int32_t height);

    /**
     * all information about the output have been sent
     *
     * This event is sent after all other properties of an xdg_output
     * have been sent.
     *
     * This allows changes to the xdg_output properties to be seen as
     * atomic, even if they happen via multiple events.
     *
     * For objects version 3 onwards, this event is deprecated.
     * Compositors are not required to send it anymore and must send
     * wl_output.done instead.
     */
    static void handle_done(void *data, struct zxdg_output_v1 *zxdg_output_v1);

    /**
     * name of this output
     *
     * Many compositors will assign names to their outputs, show them
     * to the user, allow them to be configured by name, etc. The
     * client may wish to know this name as well to offer the user
     * similar behaviors.
     *
     * The naming convention is compositor defined, but limited to
     * alphanumeric characters and dashes (-). Each name is unique
     * among all wl_output globals, but if a wl_output global is
     * destroyed the same name may be reused later. The names will also
     * remain consistent across sessions with the same hardware and
     * software configuration.
     *
     * Examples of names include 'HDMI-A-1', 'WL-1', 'X11-1', etc.
     * However, do not assume that the name is a reflection of an
     * underlying DRM connector, X11 connection, etc.
     *
     * The name event is sent after creating an xdg_output (see
     * xdg_output_manager.get_xdg_output). This event is only sent once
     * per xdg_output, and the name does not change over the lifetime
     * of the wl_output global.
     *
     * This event is deprecated, instead clients should use
     * wl_output.name. Compositors must still support this event.
     * @param name output name
     * @since 2
     */
    static void handle_name(void *data,
                            struct zxdg_output_v1 *zxdg_output_v1,
                            const char *name);

    /**
     * human-readable description of this output
     *
     * Many compositors can produce human-readable descriptions of
     * their outputs. The client may wish to know this description as
     * well, to communicate the user for various purposes.
     *
     * The description is a UTF-8 string with no convention defined for
     * its contents. Examples might include 'Foocorp 11" Display' or
     * 'Virtual X11 output via :1'.
     *
     * The description event is sent after creating an xdg_output (see
     * xdg_output_manager.get_xdg_output) and whenever the description
     * changes. The description is optional, and may not be sent at
     * all.
     *
     * For objects of version 2 and lower, this event is only sent once
     * per xdg_output, and the description does not change over the
     * lifetime of the wl_output global.
     *
     * This event is deprecated, instead clients should use
     * wl_output.description. Compositors must still support this
     * event.
     * @param description output description
     * @since 2
     */
    static void handle_description(void *data,
                                   struct zxdg_output_v1 *zxdg_output_v1,
                                   const char *description);

    static constexpr struct zxdg_output_v1_listener listener_ = {
            .logical_position = handle_logical_position,
            .logical_size = handle_logical_size,
            .done = handle_done,
            .name = handle_name,
            .description = handle_description,
    };
};
