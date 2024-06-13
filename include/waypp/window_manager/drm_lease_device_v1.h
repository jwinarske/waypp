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

#include <waypp/waypp.h>

class DrmLeaseDevice_v1 {
public:
    explicit DrmLeaseDevice_v1(
            struct wp_drm_lease_device_v1 *wp_drm_lease_device_v1);

    ~DrmLeaseDevice_v1();

    // Disallow copy and assign.
    DrmLeaseDevice_v1(const DrmLeaseDevice_v1 &) = delete;

    DrmLeaseDevice_v1 &operator=(const DrmLeaseDevice_v1 &) = delete;

private:
    struct wp_drm_lease_device_v1 *wp_drm_lease_device_v1_;
    struct wp_drm_lease_v1 *wp_drm_lease_v1_;
    struct wp_drm_lease_connector_v1 *wp_drm_lease_connector_v1_;
    int32_t leased_fd_;

    /**
     * shares the DRM file descriptor
     *
     * This event returns a file descriptor suitable for use with
     * DRM-related ioctls. The client should use drmModeGetLease to
     * enumerate the DRM objects which have been leased to them. The
     * compositor guarantees it will not use the leased DRM objects
     * itself until it sends the finished event. If the compositor
     * cannot or will not grant a lease for the requested connectors,
     * it will not send this event, instead sending the finished event.
     *
     * The compositor will send this event at most once during this
     * objects lifetime.
     * @param leased_fd leased DRM file descriptor
     */
    static void handle_lease_fd(void *data,
                                struct wp_drm_lease_v1 *wp_drm_lease_v1,
                                int32_t leased_fd);

    /**
     * sent when the lease has been revoked
     *
     * The compositor uses this event to either reject a lease
     * request, or if it previously sent a lease_fd, to notify the
     * client that the lease has been revoked. If the client requires a
     * new lease, they should destroy this object and submit a new
     * lease request. The compositor will send no further events for
     * this object after sending the finish event. Compositors should
     * revoke the lease when any of the leased resources become
     * unavailable, namely when a hot-unplug occurs or when the
     * compositor loses DRM master.
     */
    static void handle_lease_finished(void *data,
                                      struct wp_drm_lease_v1 *wp_drm_lease_v1);

    static constexpr struct wp_drm_lease_v1_listener lease_listener_ = {
            .lease_fd = handle_lease_fd,
            .finished = handle_lease_finished,
    };

    /**
     * open a non-master fd for this DRM node
     *
     * The compositor will send this event when the
     * wp_drm_lease_device_v1 global is bound, although there are no
     * guarantees as to how long this takes - the compositor might need
     * to wait until regaining DRM master. The included fd is a
     * non-master DRM file descriptor opened for this device and the
     * compositor must not authenticate it. The purpose of this event
     * is to give the client the ability to query DRM and discover
     * information which may help them pick the appropriate DRM device
     * or select the appropriate connectors therein.
     * @param fd DRM file descriptor
     */
    static void handle_device_drm_fd(
            void *data,
            struct wp_drm_lease_device_v1 *wp_drm_lease_device_v1,
            int32_t fd);

    /**
     * advertise connectors available for leases
     *
     * The compositor will use this event to advertise connectors
     * available for lease by clients. This object may be passed into a
     * lease request to indicate the client would like to lease that
     * connector, see wp_drm_lease_request_v1.request_connector for
     * details. While the compositor will make a best effort to not
     * send disconnected connectors, no guarantees can be made.
     *
     * The compositor must send the drm_fd event before sending
     * connectors. After the drm_fd event it will send all available
     * connectors but may send additional connectors at any time.
     */
    static void handle_device_connector(
            void *data,
            struct wp_drm_lease_device_v1 *wp_drm_lease_device_v1,
            struct wp_drm_lease_connector_v1 *id);

    /**
     * signals grouping of connectors
     *
     * The compositor will send this event to indicate that it has
     * sent all currently available connectors after the client binds
     * to the global or when it updates the connector list, for example
     * on hotplug, drm master change or when a leased connector becomes
     * available again. It will similarly send this event to group
     * wp_drm_lease_connector_v1.withdrawn events of connectors of this
     * device.
     */
    static void handle_device_done(
            void *data,
            struct wp_drm_lease_device_v1 *wp_drm_lease_device_v1);

    /**
     * the compositor has finished using the device
     *
     * This event is sent in response to the release request and
     * indicates that the compositor is done sending connector events.
     * The compositor will destroy this object immediately after
     * sending the event and it will become invalid. The client should
     * release any resources associated with this device after
     * receiving this event.
     */
    static void handle_device_released(
            void *data,
            struct wp_drm_lease_device_v1 *wp_drm_lease_device_v1);

    static constexpr struct wp_drm_lease_device_v1_listener device_listener_ = {
            .drm_fd = handle_device_drm_fd,
            .connector = handle_device_connector,
            .done = handle_device_done,
            .released = handle_device_released,
    };

    /**
     * name
     *
     * The compositor sends this event once the connector is created
     * to indicate the name of this connector. This will not change for
     * the duration of the Wayland session, but is not guaranteed to be
     * consistent between sessions.
     *
     * If the compositor supports wl_output version 4 and this
     * connector corresponds to a wl_output, the compositor should use
     * the same name as for the wl_output.
     * @param name connector name
     */
    static void handle_connector_name(
            void *data,
            struct wp_drm_lease_connector_v1 *wp_drm_lease_connector_v1,
            const char *name);

    /**
     * description
     *
     * The compositor sends this event once the connector is created
     * to provide a human-readable description for this connector,
     * which may be presented to the user. The compositor may send this
     * event multiple times over the lifetime of this object to reflect
     * changes in the description.
     * @param description connector description
     */
    static void handle_connector_description(
            void *data,
            struct wp_drm_lease_connector_v1 *wp_drm_lease_connector_v1,
            const char *description);

    /**
     * connector_id
     *
     * The compositor sends this event once the connector is created
     * to indicate the DRM object ID which represents the underlying
     * connector that is being offered. Note that the final lease may
     * include additional object IDs, such as CRTCs and planes.
     * @param connector_id DRM connector ID
     */
    static void handle_connector_connector_id(
            void *data,
            struct wp_drm_lease_connector_v1 *wp_drm_lease_connector_v1,
            uint32_t connector_id);

    /**
     * all properties have been sent
     *
     * This event is sent after all properties of a connector have
     * been sent. This allows changes to the properties to be seen as
     * atomic even if they happen via multiple events.
     */
    static void handle_connector_done(
            void *data,
            struct wp_drm_lease_connector_v1 *wp_drm_lease_connector_v1);

    /**
     * lease offer withdrawn
     *
     * Sent to indicate that the compositor will no longer honor
     * requests for DRM leases which include this connector. The client
     * may still issue a lease request including this connector, but
     * the compositor will send wp_drm_lease_v1.finished without
     * issuing a lease fd. Compositors are encouraged to send this
     * event when they lose access to connector, for example when the
     * connector is hot-unplugged, when the connector gets leased to a
     * client or when the compositor loses DRM master.
     */
    static void handle_connector_withdrawn(
            void *data,
            struct wp_drm_lease_connector_v1 *wp_drm_lease_connector_v1);

    static constexpr struct wp_drm_lease_connector_v1_listener
            connector_listener_ = {
            .name = handle_connector_name,
            .description = handle_connector_description,
            .connector_id = handle_connector_connector_id,
            .done = handle_connector_done,
            .withdrawn = handle_connector_withdrawn,
    };
};