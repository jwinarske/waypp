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

#include "waypp/window_manager/xdg_output.h"

#include "logging/logging.h"

XdgOutput::XdgOutput(wl_proxy* zxdg_output_manager_v1,
                     wl_output* wl_output) {
  zxdg_output_v1_ = wl_proxy_marshal_constructor(
      zxdg_output_manager_v1,
      xdg_output_unstable_v1::client::zxdg_output_manager_v1_traits::Op::GetXdgOutput,
      &xdg_output_unstable_v1::client::zxdg_output_v1_traits::wl_iface(),
      nullptr, (wl_proxy*)wl_output);
  wl_proxy_add_listener(zxdg_output_v1_,
                        reinterpret_cast<void(**)(void)>(
                            const_cast<void**>(listener_)),
                        this);
}

XdgOutput::~XdgOutput() {
  if (zxdg_output_v1_) {
    DLOG_TRACE("[Registrar] zxdg_output_v1_destroy(zxdg_output_v1_)");
    wl_proxy_destroy(zxdg_output_v1_);
  }
}

void XdgOutput::handle_logical_position(void* data,
                                        wl_proxy* /*zxdg_output_v1*/,
                                        int32_t x,
                                        int32_t y) {
  auto obj = static_cast<XdgOutput*>(data);
  LOG_DEBUG("XdgOutput::handle_logical_position: x: {}, y: {}", x, y);

  obj->output_.logical_position = {
      .x = x,
      .y = y,
  };
}

void XdgOutput::handle_logical_size(void* data,
                                    wl_proxy* /*zxdg_output_v1*/,
                                    int32_t width,
                                    int32_t height) {
  auto obj = static_cast<XdgOutput*>(data);
  LOG_DEBUG("XdgOutput::handle_logical_size: width: {}, height: {}", width,
            height);

  obj->output_.logical_size = {
      .width = width,
      .height = height,
  };
}

void XdgOutput::handle_done(void* data, wl_proxy* /*zxdg_output_v1*/) {
  auto obj = static_cast<XdgOutput*>(data);
  LOG_DEBUG("XdgOutput::handle_done");
  obj->output_.done = true;
}

void XdgOutput::handle_name(void* data,
                            wl_proxy* /*zxdg_output_v1*/,
                            const char* name) {
  auto obj = static_cast<XdgOutput*>(data);
  LOG_DEBUG("XdgOutput::handle_name: {}", name);
  obj->output_.name = name;
}

void XdgOutput::handle_description(void* data,
                                   wl_proxy* /*zxdg_output_v1*/,
                                   const char* description) {
  auto obj = static_cast<XdgOutput*>(data);
  LOG_DEBUG("XdgOutput::handle_description: {}", description);
  obj->output_.description = description;
}

void XdgOutput::print() const {
  LOG_INFO("XDG Output");
  LOG_INFO("\tDone: {}", output_.done);
  LOG_INFO("\tName: {}", output_.name);
  LOG_INFO("\tDescription: {}", output_.description);
  LOG_INFO("\tlogical_position");
  LOG_INFO("\t\tx: {}", output_.logical_position.x);
  LOG_INFO("\t\ty: {}", output_.logical_position.y);
  LOG_INFO("\tlogical_size");
  LOG_INFO("\t\tx: {}", output_.logical_size.width);
  LOG_INFO("\t\ty: {}", output_.logical_size.height);
}
