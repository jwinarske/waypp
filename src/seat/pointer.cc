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

#include "pointer.h"

#include <algorithm>

#include <wayland-client.h>
#include <wayland-cursor.h>

#include "command.h"
#include "logging.h"

/**
 * @brief Pointer class represents a Wayland pointer device.
 *
 * The Pointer class is responsible for handling Wayland pointer events and
 * managing the cursor.
 */
Pointer::Pointer(wl_pointer* pointer,
                 struct wl_compositor* wl_compositor,
                 struct wl_shm* wl_shm,
                 bool disable_cursor,
                 int size)
    : wl_pointer_(pointer),
      wl_shm_(wl_shm),
      disable_cursor_(disable_cursor),
      size_(size) {
  SPDLOG_DEBUG("Pointer");
  wl_pointer_add_listener(pointer, &pointer_listener_, this);
  wl_surface_cursor_ = wl_compositor_create_surface(wl_compositor);
}

/**
 * @class Pointer
 * Represents a WL_POINTER object and handles various pointer events.
 * This class is responsible for releasing and destroying the WL_POINTER object,
 * as well as managing the Cursor object associated with the pointer.
 *
 * @param pointer_ A pointer to the WL_POINTER object.
 * @param shm A pointer to the WL_SHM object.
 * @param compositor A pointer to the WL_COMPOSITOR object.
 * @param enable_cursor A boolean flag indicating whether to enable cursor.
 */
Pointer::~Pointer() {
  if (theme_) {
    wl_cursor_theme_destroy(theme_);
  }
  if (wl_surface_cursor_) {
    wl_surface_destroy(wl_surface_cursor_);
  }
  if (wl_pointer_) {
    wl_pointer_release(wl_pointer_);
  }
}

/**
 * @class Pointer
 * @brief A class that handles pointer events
 *
 * This class provides functionality to handle various pointer events,
 * such as enter, leave, motion, button, axis, frame, axis source, axis stop,
 * and axis discrete events.
 */
void Pointer::handle_enter(void* data,
                           struct wl_pointer* pointer,
                           uint32_t serial,
                           struct wl_surface* surface,
                           wl_fixed_t sx,
                           wl_fixed_t sy) {
  auto obj = static_cast<Pointer*>(data);
  if (obj->wl_pointer_ != pointer) {
    return;
  }

  SPDLOG_TRACE("Pointer::handle_enter");

  for (auto observer : obj->observers_) {
    observer->notify_pointer_enter(obj, pointer, serial, surface,
                                   wl_fixed_to_double(sx),
                                   wl_fixed_to_double(sy));
  }
}

/**
 * @brief Handles the leave event of the pointer.
 *
 * This function is called when the pointer leaves a surface.
 * It outputs a debug message to the standard error stream.
 *
 * @param data A pointer to user-defined data.
 * @param pointer The pointer object.
 * @param serial The serial number of the event.
 * @param surface The surface that the pointer left.
 */
void Pointer::handle_leave(void* data,
                           struct wl_pointer* pointer,
                           uint32_t serial,
                           struct wl_surface* surface) {
  auto obj = static_cast<Pointer*>(data);
  if (obj->wl_pointer_ != pointer) {
    return;
  }

  SPDLOG_TRACE("Pointer::handle_leave");

  for (auto observer : obj->observers_) {
    observer->notify_pointer_leave(obj, pointer, serial, surface);
  }
}

/**
 * @brief Handles motion events from the pointer device.
 *
 * This function is a callback that is called when a motion
 * event occurs on the pointer device.
 *
 * @param data A pointer to user data.
 * @param pointer The pointer object that triggered the event.
 * @param time The timestamp of the event.
 * @param sx The X coordinate of the pointer's absolute position.
 * @param sy The Y coordinate of the pointer's absolute position.
 */
void Pointer::handle_motion(void* data,
                            struct wl_pointer* pointer,
                            uint32_t time,
                            wl_fixed_t sx,
                            wl_fixed_t sy) {
  auto obj = static_cast<Pointer*>(data);
  if (obj->wl_pointer_ != pointer) {
    return;
  }

  SPDLOG_TRACE("Pointer::handle_motion");

  for (auto observer : obj->observers_) {
    observer->notify_pointer_motion(obj, pointer, time, sx, sy);
  }
}

/**
 * @brief Function to handle button events from the pointer
 *
 * @param data A pointer to user-defined data
 * @param wl_pointer The Wayland pointer object
 * @param serial The serial number of the event
 * @param button The button that triggered the event
 * @param state The state of the button (pressed or released)
 */
void Pointer::handle_button(void* data,
                            struct wl_pointer* pointer,
                            uint32_t serial,
                            uint32_t time,
                            uint32_t button,
                            uint32_t state) {
  auto obj = static_cast<Pointer*>(data);
  if (obj->wl_pointer_ != pointer) {
    return;
  }

  SPDLOG_TRACE("Pointer::handle_button");

  for (auto observer : obj->observers_) {
    observer->notify_pointer_button(obj, pointer, serial, time, button, state);
  }
}

/**
 * @brief Handles the axis event of the pointer.
 *
 * This function is called when the pointer generates an axis event, such as a
 * scroll event.
 *
 * @param data      A pointer to user-defined data.
 * @param wl_pointer    A pointer to the wl_pointer object that triggered the
 * event.
 * @param time      The timestamp of the event.
 * @param axis      The axis identifier.
 * @param value     The value of the axis event.
 *
 * @details Prints "Pointer::handle_axis" to the standard error output.
 */
void Pointer::handle_axis(void* data,
                          struct wl_pointer* pointer,
                          uint32_t time,
                          uint32_t axis,
                          wl_fixed_t value) {
  auto obj = static_cast<Pointer*>(data);
  if (obj->wl_pointer_ != pointer) {
    return;
  }

  SPDLOG_TRACE("Pointer::handle_axis");

  for (auto observer : obj->observers_) {
    observer->notify_pointer_axis(obj, pointer, time, axis, value);
  }
}

/**
 * @brief Handle a frame event for the pointer.
 *
 * This function is called when a frame event is received for the pointer.
 *
 * @param data The user data associated with the pointer.
 * @param wl_pointer The pointer object.
 */
void Pointer::handle_frame(void* data, struct wl_pointer* pointer) {
  auto obj = static_cast<Pointer*>(data);
  if (obj->wl_pointer_ != pointer) {
    return;
  }

  SPDLOG_TRACE("Pointer::handle_frame");

  for (auto observer : obj->observers_) {
    observer->notify_pointer_frame(obj, pointer);
  }
}

/**
 * @brief Handles the axis source event for the Pointer object.
 *
 * @param data Unused parameter.
 * @param wl_pointer The wl_pointer object associated with the event.
 * @param axis_source The axis source.
 *
 * This function is called when the axis source event is received for the
 * Pointer object. It prints a message to the standard error stream.
 */
void Pointer::handle_axis_source(void* data,
                                 struct wl_pointer* pointer,
                                 uint32_t axis_source) {
  auto obj = static_cast<Pointer*>(data);
  if (obj->wl_pointer_ != pointer) {
    return;
  }

  SPDLOG_TRACE("Pointer::handle_axis_source");

  for (auto observer : obj->observers_) {
    observer->notify_pointer_axis_source(obj, pointer, axis_source);
  }
}

/**
 * @brief Handles the stop event for an axis on the pointer.
 *
 * This function is called when an axis stop event is received for the pointer.
 *
 * @param data      A pointer to user-defined data.
 * @param wl_pointer The pointer object that triggered the event.
 * @param time      The timestamp of the event.
 * @param axis      The axis that stopped.
 */
void Pointer::handle_axis_stop(void* data,
                               struct wl_pointer* pointer,
                               uint32_t time,
                               uint32_t axis) {
  auto obj = static_cast<Pointer*>(data);
  if (obj->wl_pointer_ != pointer) {
    return;
  }

  SPDLOG_TRACE("Pointer::handle_axis_stop");

  for (auto observer : obj->observers_) {
    observer->notify_pointer_axis_stop(obj, pointer, time, axis);
  }
}

/**
 * @brief Handles the discrete axis events for the Pointer.
 *
 * This function is called when a discrete axis event is received.
 *
 * @param data The user data associated with the Pointer.
 * @param wl_pointer The pointer object.
 * @param axis The axis value.
 * @param discrete The discrete value.
 */
void Pointer::handle_axis_discrete(void* data,
                                   struct wl_pointer* pointer,
                                   uint32_t axis,
                                   int32_t discrete) {
  auto obj = static_cast<Pointer*>(data);
  if (obj->wl_pointer_ != pointer) {
    return;
  }

  SPDLOG_TRACE("Pointer::handle_axis_discrete");

  for (auto observer : obj->observers_) {
    observer->notify_pointer_axis_discrete(obj, pointer, axis, discrete);
  }
}

void Pointer::set_cursor(uint32_t serial,
                         const char* cursor_name,
                         const char* theme_name) {
  if (disable_cursor_) {
    wl_pointer_set_cursor(wl_pointer_, serial, wl_surface_cursor_, 0, 0);
    wl_surface_damage(wl_surface_cursor_, 0, 0, 0, 0);
    wl_surface_commit(wl_surface_cursor_);
    return;
  }

  if (!wl_shm_) {
    return;
  }

  if (!theme_) {
    theme_ = wl_cursor_theme_load(theme_name, size_, wl_shm_);
    if (!theme_) {
      spdlog::error("[Pointer] unable to load {} theme",
                    theme_name == nullptr ? "default" : theme_name);
      return;
    }
  }

  auto cursor = wl_cursor_theme_get_cursor(theme_, cursor_name);
  if (!cursor) {
    spdlog::error("[Pointer] unable to load {}", cursor_name);
    return;
  }
  auto image = cursor->images[0];
  auto buffer = wl_cursor_image_get_buffer(image);
  if (!buffer) {
    return;
  }
  wl_pointer_set_cursor(wl_pointer_, serial, wl_surface_cursor_,
                        static_cast<int32_t>(image->hotspot_x),
                        static_cast<int32_t>(image->hotspot_y));
  wl_surface_attach(wl_surface_cursor_, buffer, 0, 0);
  wl_surface_damage(wl_surface_cursor_, 0, 0,
                    static_cast<int32_t>(image->width),
                    static_cast<int32_t>(image->height));
  wl_surface_commit(wl_surface_cursor_);
}

std::string Pointer::get_cursor_theme() {
  std::string res;
  Command::Execute("gsettings get org.gnome.desktop.interface cursor-theme",
                   res);

  if (!res.empty()) {
    // clean up string
    std::string tmp = "\'\n";
    for_each(tmp.begin(), tmp.end(), [&res](char n) {
      res.erase(std::remove(res.begin(), res.end(), n), res.end());
    });
  }

  return std::move(res);
}

std::vector<std::string> Pointer::get_available_cursors(
    const char* theme_name) {
  std::string theme = theme_name == nullptr ? get_cursor_theme() : theme_name;

  std::ostringstream ss;
  ss << "ls -1 /usr/share/icons/" << theme << "/cursors";

  std::string res;
  Command::Execute(ss.str().c_str(), res);

  std::vector<std::string> cursor_list;

  std::string line;
  std::istringstream orig_stream(res);
  while (std::getline(orig_stream, line)) {
    if (!line.empty())
      cursor_list.push_back(line);
  }

  std::sort(cursor_list.begin(), cursor_list.end());

  return std::move(cursor_list);
}
