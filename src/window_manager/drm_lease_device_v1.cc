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

#include "drm_lease_device_v1.h"

#include "logging.h"

DrmLeaseDevice_v1::DrmLeaseDevice_v1(
    struct wp_drm_lease_device_v1* wp_drm_lease_device_v1)
    : wp_drm_lease_device_v1_(wp_drm_lease_device_v1) {
  wp_drm_lease_device_v1_add_listener(wp_drm_lease_device_v1_,
                                      &device_listener_, this);
}

DrmLeaseDevice_v1::~DrmLeaseDevice_v1() {
  wp_drm_lease_device_v1_destroy(wp_drm_lease_device_v1_);
}

void DrmLeaseDevice_v1::handle_device_drm_fd(
    void* data,
    struct wp_drm_lease_device_v1* wp_drm_lease_device_v1,
    int32_t fd) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_device_v1_ != wp_drm_lease_device_v1) {
    return;
  }
  SPDLOG_DEBUG("handle_device_drm_fd: fd: {}", fd);
}

void DrmLeaseDevice_v1::handle_device_connector(
    void* data,
    struct wp_drm_lease_device_v1* wp_drm_lease_device_v1,
    struct wp_drm_lease_connector_v1* id) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_device_v1_ != wp_drm_lease_device_v1) {
    return;
  }
  SPDLOG_DEBUG("handle_device_connector: fd: 0x{:x}", fmt::ptr(id));
}

void DrmLeaseDevice_v1::handle_device_done(
    void* data,
    struct wp_drm_lease_device_v1* wp_drm_lease_device_v1) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_device_v1_ != wp_drm_lease_device_v1) {
    return;
  }
  SPDLOG_DEBUG("handle_device_done");
}

void DrmLeaseDevice_v1::handle_device_released(
    void* data,
    struct wp_drm_lease_device_v1* wp_drm_lease_device_v1) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_device_v1_ != wp_drm_lease_device_v1) {
    return;
  }
  SPDLOG_DEBUG("handle_device_released");
}

void DrmLeaseDevice_v1::handle_connector_name(
    void* data,
    struct wp_drm_lease_connector_v1* wp_drm_lease_connector_v1,
    const char* name) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_connector_v1_ != wp_drm_lease_connector_v1) {
    return;
  }
  SPDLOG_DEBUG("handle_connector_name: name: {}", name);
}

void DrmLeaseDevice_v1::handle_connector_description(
    void* data,
    struct wp_drm_lease_connector_v1* wp_drm_lease_connector_v1,
    const char* description) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_connector_v1_ != wp_drm_lease_connector_v1) {
    return;
  }
  SPDLOG_DEBUG("handle_connector_description: description: {}", description);
}

void DrmLeaseDevice_v1::handle_connector_connector_id(
    void* data,
    struct wp_drm_lease_connector_v1* wp_drm_lease_connector_v1,
    uint32_t connector_id) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_connector_v1_ != wp_drm_lease_connector_v1) {
    return;
  }
  SPDLOG_DEBUG("handle_connector_connector_id: connector_id: {}", connector_id);
}

void DrmLeaseDevice_v1::handle_connector_done(
    void* data,
    struct wp_drm_lease_connector_v1* wp_drm_lease_connector_v1) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_connector_v1_ != wp_drm_lease_connector_v1) {
    return;
  }
  SPDLOG_DEBUG("handle_connector_done");
}

void DrmLeaseDevice_v1::handle_connector_withdrawn(
    void* data,
    struct wp_drm_lease_connector_v1* wp_drm_lease_connector_v1) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_connector_v1_ != wp_drm_lease_connector_v1) {
    return;
  }
  SPDLOG_DEBUG("handle_connector_withdrawn");
}

void DrmLeaseDevice_v1::handle_lease_fd(void* data,
                                        struct wp_drm_lease_v1* wp_drm_lease_v1,
                                        int32_t leased_fd) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_v1_ != wp_drm_lease_v1) {
    return;
  }
  obj->leased_fd_ = leased_fd;
  SPDLOG_DEBUG("handle_lease_fd: leased_fd: {}", leased_fd);
}

void DrmLeaseDevice_v1::handle_lease_finished(
    void* data,
    struct wp_drm_lease_v1* wp_drm_lease_v1) {
  auto obj = static_cast<DrmLeaseDevice_v1*>(data);
  if (obj->wp_drm_lease_v1_ != wp_drm_lease_v1) {
    return;
  }
  SPDLOG_DEBUG("handle_lease_finished");
}
