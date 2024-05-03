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

#include "agl_shell.h"

#include "logging.h"


/**
 * @class AglShell
 *
 * @brief AglShell represents a Shell  for a Wayland-based display.
 *
 * The AglShell class is responsible for managing application windows using the XDG Shell protocol.
 */
AglShell::AglShell(bool disable_cursor, unsigned long ext_interface_count,
                   const Registrar::RegistrarCallback *ext_interface_data,
                   GMainContext *context,
                   const char *display_name) : XdgWindowManager(
        disable_cursor, ext_interface_count,
        ext_interface_data,
        context,
        display_name),
                                               wait_for_bound_(true),
                                               bound_ok_(false) {
    auto agl_shell = get_agl_shell();
    if (!agl_shell.has_value()) {
        spdlog::critical("{} is required.", agl_shell_interface.name);
        exit(EXIT_FAILURE);
    }
    agl_shell_ = agl_shell.value();

    agl_shell_add_listener(agl_shell_, &agl_shell_listener_, this);

    int ret = 0;
    while (ret != -1 && wait_for_bound_) {
        ret = wl_display_dispatch(get_display());
        if (wait_for_bound_)
            continue;
    }
    if (!bound_ok_) {
        spdlog::critical(
                "agl_shell extension already in use by other shell client.");
        exit(EXIT_FAILURE);
    }
}

AglShell::~AglShell() = default;

void AglShell::handle_bound_ok(void *data,
                               struct agl_shell *agl_shell) {
    auto *obj = static_cast<AglShell *>(data);
    if (obj->agl_shell_ != agl_shell) {
        return;
    }

    SPDLOG_DEBUG("AglShell::handle_bound_ok");

    obj->wait_for_bound_ = false;
    obj->bound_ok_ = true;
}

struct wl_output *AglShell::find_output_by_name(const std::string &output_name) {
    for (auto &it: output_.outputs) {
        if (it.second->get_name() == output_name) {
            return it.first;
        }
    }
    return nullptr;
}

void AglShell::activate_app(const std::string &app_id) {

    SPDLOG_DEBUG("got app_id {}", app_id);

    // search for a pending application which might have a different output
    auto iter = pending_app_list_.begin();
    bool found_pending_app = false;
    while (iter != pending_app_list_.end()) {
        auto app_to_search = iter->first;
        SPDLOG_DEBUG("searching for {}", app_to_search);

        if (app_to_search == app_id) {
            found_pending_app = true;
            break;
        }

        iter++;
    }

    std::string output_name;
    struct wl_output *wl_output{};
    if (found_pending_app) {
        output_name = iter->second;
        wl_output = find_output_by_name(output_name);

        SPDLOG_DEBUG("Found app_id {} at all", app_id);

        if (!wl_output) {
            // try with remoting-remote-X which is the streaming
            wl_output = find_output_by_name("remoting-" + output_name);
            if (!wl_output) {
                SPDLOG_DEBUG("Not activating app_id {} at all", app_id);
                return;
            }
        }

        pending_app_list_.erase(iter);
    }

    SPDLOG_DEBUG("Activating app_id {} on output {}", app_id, output_name);
    agl_shell_activate_app(agl_shell_, app_id.c_str(), wl_output);
    wl_display_flush(get_display());
}

void AglShell::deactivate_app(const std::string &app_id) {
    for (auto &i: apps_stack_) {
        if (i == app_id) {
            // remove it from apps_stack
            apps_stack_.remove(i);
            if (!apps_stack_.empty())
                activate_app(apps_stack_.back());
            break;
        }
    }
}

void AglShell::add_app_to_stack(const std::string &app_id) {
    bool found_app = false;
    for (auto &i: apps_stack_) {
        if (i == app_id) {
            found_app = true;
            break;
        }
    }

    if (!found_app) {
        apps_stack_.push_back(app_id);
    }
}

void AglShell::process_app_status_event(const char *app_id, const std::string &event_type) {

    if (event_type == "started") {
        activate_app(std::string(app_id));
    } else if (event_type == "terminated") {
        deactivate_app(std::string(app_id));
    } else if (event_type == "deactivated") {
        // not handled
    }
}

void AglShell::handle_bound_fail(void *data,
                                 struct agl_shell *agl_shell) {
    auto *obj = static_cast<AglShell *>(data);
    if (obj->agl_shell_ != agl_shell) {
        return;
    }

    SPDLOG_DEBUG("AglShell::handle_bound_fail");

    obj->wait_for_bound_ = false;
    obj->bound_ok_ = false;
}

void AglShell::handle_app_state(void *data,
                                struct agl_shell *agl_shell,
                                const char *app_id,
                                uint32_t state) {
    auto *obj = static_cast<AglShell *>(data);
    if (obj->agl_shell_ != agl_shell) {
        return;
    }

    SPDLOG_DEBUG("AglShell::handle_app_state");

    switch (state) {
        case AGL_SHELL_APP_STATE_STARTED:
            SPDLOG_DEBUG("[AGL] AGL_SHELL_APP_STATE_STARTED for app_id {}", app_id);
            obj->process_app_status_event(app_id, std::string("started"));
            break;
        case AGL_SHELL_APP_STATE_TERMINATED:
            SPDLOG_DEBUG("[AGL] AGL_SHELL_APP_STATE_TERMINATED for app_id {}", app_id);
            break;
        case AGL_SHELL_APP_STATE_ACTIVATED:
            SPDLOG_DEBUG("[AGL] AGL_SHELL_APP_STATE_ACTIVATED for app_id {}", app_id);
            obj->add_app_to_stack(std::string(app_id));
            break;
        case AGL_SHELL_APP_STATE_DEACTIVATED:
            obj->process_app_status_event(app_id, std::string("deactivated"));
            break;
        default:
            break;
    }
}

void AglShell::handle_app_on_output(void *data,
                                    struct agl_shell *agl_shell,
                                    const char *app_id,
                                    const char *output_name) {
    auto *obj = static_cast<AglShell *>(data);
    if (obj->agl_shell_ != agl_shell) {
        return;
    }

    SPDLOG_DEBUG("[AGL] app_on_out app_id {} output name {}", app_id, output_name);

    // a couple of use-cases, if there is no app_id in the app_list then it
    // means this is a request to map the application, from the start to a
    // different output that the default one. We'd get an
    // AGL_SHELL_APP_STATE_STARTED which will handle activation.
    //
    // if there's an app_id then it means we might have gotten an event to
    // move the application to another output; so we'd need to process it
    // by explicitly calling processAppStatusEvent() which would ultimately
    // activate the application on other output. We'd have to pick-up the
    // last activated surface and activate the default output.
    //
    // finally if the outputs are identical probably that's an user-error -
    // but the compositor won't activate it again, so we don't handle that.
    std::pair new_pending_app =
            std::pair(std::string(app_id), std::string(output_name));
    obj->pending_app_list_.emplace_back(new_pending_app);

    auto iter = obj->apps_stack_.begin();
    while (iter != obj->apps_stack_.end()) {
        if (*iter == std::string(app_id)) {
            SPDLOG_DEBUG("[AGL] move {} to another output {}", app_id, output_name);
            obj->process_app_status_event(app_id, std::string("started"));
            break;
        }
        iter++;
    }
}

void AglShell::set_background(struct wl_surface *wl_surface, struct wl_output *wl_output) const {
    agl_shell_set_background(agl_shell_, wl_surface, wl_output);
}

void
AglShell::set_panel(struct wl_surface *wl_surface, struct wl_output *wl_output, const enum agl_shell_edge mode) const {
    agl_shell_set_panel(agl_shell_, wl_surface, wl_output, mode);
}

void AglShell::set_activate_area(struct wl_output *wl_output,
                                 uint32_t x,
                                 uint32_t y,
                                 uint32_t width,
                                 uint32_t height) const {
    SPDLOG_DEBUG("Using custom rectangle [{}x{}+{}x{}] for activation", width,
                 height, x, y);

    agl_shell_set_activate_region(
            agl_shell_, wl_output, static_cast<int32_t>(x),
            static_cast<int32_t>(y), static_cast<int32_t>(width),
            static_cast<int32_t>(height));
}

void AglShell::ready() const {
    agl_shell_ready(agl_shell_);
}
