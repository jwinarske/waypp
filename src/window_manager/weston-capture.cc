
#include "waypp/window_manager/weston-capture.h"

#include "logging/logging.h"

WestonCapture::WestonCapture(weston_capture_v1 *weston_capture_v1,
                             wl_output *wl_output,
                             weston_capture_v1_source source,
                             WestonCaptureObserver *observer,
                             void *user_data)
        : weston_capture_v1_(weston_capture_v1),
          wl_output_(wl_output),
          source_(source),
          user_data_(user_data) {
    if (observer) {
        register_observer(observer);
    }

    weston_capture_source_v1_ =
            weston_capture_v1_create(weston_capture_v1_, wl_output_, source_);
    weston_capture_source_v1_add_listener(weston_capture_source_v1_, &listener_,
                                          this);
}

WestonCapture::~WestonCapture() = default;

void WestonCapture::handle_format(
        void *data,
        weston_capture_source_v1 *weston_capture_source_v1,
        uint32_t drm_format) {
    auto obj = static_cast<WestonCapture *>(data);
    if (obj->weston_capture_source_v1_ != weston_capture_source_v1) {
        return;
    }

    DLOG_TRACE("WestonCapture: drm_format: {}", drm_format);

    for (auto observer: obj->observers_) {
        observer->notify_weston_capture_format(
                obj->user_data_, weston_capture_source_v1, drm_format);
    }
}

void WestonCapture::handle_size(
        void *data,
        weston_capture_source_v1 *weston_capture_source_v1,
        int32_t width,
        int32_t height) {
    auto obj = static_cast<WestonCapture *>(data);
    if (obj->weston_capture_source_v1_ != weston_capture_source_v1) {
        return;
    }

    DLOG_TRACE("WestonCapture: size: width: {}, height: {}", width, height);

    for (auto observer: obj->observers_) {
        observer->notify_weston_capture_size(
                obj->user_data_, weston_capture_source_v1, width, height);
    }
}

void WestonCapture::handle_complete(
        void *data,
        weston_capture_source_v1 *weston_capture_source_v1) {
    auto obj = static_cast<WestonCapture *>(data);
    if (obj->weston_capture_source_v1_ != weston_capture_source_v1) {
        return;
    }

    DLOG_TRACE("WestonCapture: complete");

    for (auto observer: obj->observers_) {
        observer->notify_weston_capture_complete(obj->user_data_,
                                                 weston_capture_source_v1);
    }
}

void WestonCapture::handle_retry(
        void *data,
        weston_capture_source_v1 *weston_capture_source_v1) {
    auto obj = static_cast<WestonCapture *>(data);
    if (obj->weston_capture_source_v1_ != weston_capture_source_v1) {
        return;
    }

    DLOG_TRACE("WestonCapture: retry");

    for (auto observer: obj->observers_) {
        observer->notify_weston_capture_retry(obj->user_data_,
                                              weston_capture_source_v1);
    }
}

void WestonCapture::handle_failed(
        void *data,
        weston_capture_source_v1 *weston_capture_source_v1,
        const char *msg) {
    auto obj = static_cast<WestonCapture *>(data);
    if (obj->weston_capture_source_v1_ != weston_capture_source_v1) {
        return;
    }

    DLOG_TRACE("WestonCapture: failed: {}", msg);

    for (auto observer: obj->observers_) {
        observer->notify_weston_capture_failed(obj->user_data_,
                                               weston_capture_source_v1, msg);
    }
}
