
#pragma once

#include <list>

#include <cstdint>

#include "wayland-protocols.h"

class WestonCapture;

class WestonCaptureObserver {
public:
    virtual ~WestonCaptureObserver() = default;

    virtual void notify_weston_capture_format(void *data,
                                              struct weston_capture_source_v1 *weston_capture_source_v1,
                                              uint32_t drm_format) = 0;

    virtual void notify_weston_capture_size(void *data,
                                            struct weston_capture_source_v1 *weston_capture_source_v1,
                                            int32_t width,
                                            int32_t height) = 0;

    virtual void notify_weston_capture_complete(void *data,
                                                struct weston_capture_source_v1 *weston_capture_source_v1) = 0;

    virtual void notify_weston_capture_retry(void *data,
                                             struct weston_capture_source_v1 *weston_capture_source_v1) = 0;

    virtual void notify_weston_capture_failed(void *data,
                                              struct weston_capture_source_v1 *weston_capture_source_v1,
                                              const char *msg) = 0;
};

class WestonCapture {
public:
    WestonCapture(struct weston_capture_v1 *weston_capture_v1, struct wl_output *output,
                  enum weston_capture_v1_source source, WestonCaptureObserver *observer = nullptr, void *user_data = nullptr);

    ~WestonCapture();

    void register_observer(WestonCaptureObserver *observer) {
        observers_.push_back(observer);
    }

    void unregister_observer(WestonCaptureObserver *observer) {
        observers_.remove(observer);
    }

    [[nodiscard]] void *get_user_data() const { return user_data_; }

    void set_user_data(void *user_data) { user_data_ = user_data; }

private:
    struct weston_capture_v1 *weston_capture_v1_;
    struct wl_output *wl_output_;
    uint32_t source_;
    void *user_data_;

    struct weston_capture_source_v1 *weston_capture_source_v1_;
    std::list<WestonCaptureObserver *> observers_{};

    /**
     * pixel format for a buffer
     *
     * This event delivers the pixel format that should be used for
     * the image buffer. Any buffer is incompatible if it does not have
     * this pixel format.
     *
     * The format modifier is linear (DRM_FORMAT_MOD_LINEAR).
     *
     * This is an initial event, and sent whenever the required format
     * changes.
     * @param drm_format DRM pixel format code
     */
    static void handle_format(void *data,
                              struct weston_capture_source_v1 *weston_capture_source_v1,
                              uint32_t drm_format);

    /**
     * dimensions for a buffer
     *
     * This event delivers the size that should be used for the image
     * buffer. Any buffer is incompatible if it does not have this
     * size.
     *
     * Row alignment of the buffer must be 4 bytes, and it must not
     * contain further row padding. Otherwise the buffer is
     * unsupported.
     *
     * This is an initial event, and sent whenever the required size
     * changes.
     * @param width width in pixels
     * @param height height in pixels
     */
    static void handle_size(void *data,
                            struct weston_capture_source_v1 *weston_capture_source_v1,
                            int32_t width,
                            int32_t height);

    /**
     * capture has completed
     *
     * This event is emitted as a response to 'capture' request when
     * it has successfully completed.
     *
     * If the buffer used in the shot is a dmabuf, the client also
     * needs to wait for any implicit fences on it before accessing the
     * contents.
     */
    static void handle_complete(void *data,
                                struct weston_capture_source_v1 *weston_capture_source_v1);

    /**
     * retry image capture with a different buffer
     *
     * This event is emitted as a response to 'capture' request when
     * it cannot succeed due to an incompatible buffer. The client has
     * already received the events delivering the new buffer
     * parameters. The client should retry the capture with the new
     * buffer parameters.
     */
    static void handle_retry(void *data,
                             struct weston_capture_source_v1 *weston_capture_source_v1);

    /**
     * capture failed
     *
     * This event is emitted as a response to 'capture' request when
     * it has failed for reasons other than an incompatible buffer. The
     * reasons may include: unsupported buffer type, unsupported buffer
     * stride, unsupported image source, the image source (output) was
     * removed, or compositor policy denied the capture.
     *
     * The string 'msg' may contain a human-readable explanation of the
     * failure to aid debugging.
     * @param msg human-readable hint
     */
    static void handle_failed(void *data,
                              struct weston_capture_source_v1 *weston_capture_source_v1,
                              const char *msg);


    static constexpr struct weston_capture_source_v1_listener listener_ = {
            .format = handle_format,
            .size = handle_size,
            .complete = handle_complete,
            .retry = handle_retry,
            .failed = handle_failed,
    };
};
