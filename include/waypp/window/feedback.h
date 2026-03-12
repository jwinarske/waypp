#pragma once

#include <waypp/waypp.h>
#include <atomic>
#include <functional>

#include <ctime>

class Feedback;

class FeedbackObserver {
 public:
  virtual ~FeedbackObserver() = default;

  virtual void notify_feedback_sync_output(
      Feedback* feedback,
      wl_proxy* wp_presentation_feedback,
      wl_output* output) = 0;

  virtual void notify_feedback_presented(
      Feedback* feedback,
      wl_proxy* wp_presentation_feedback,
      uint32_t tv_sec_hi,
      uint32_t tv_sec_lo,
      uint32_t tv_nano_sec,
      uint32_t refresh,
      uint32_t seq_hi,
      uint32_t seq_lo,
      uint32_t flags,
      timespec committed,
      timespec presented,
      uint32_t frame_stamp,
      unsigned frame_no) = 0;

  virtual void notify_feedback_discarded(
      void* data,
      wl_proxy* wp_presentation_feedback) = 0;
};

class Feedback {
 public:
  Feedback(wl_proxy* wp_presentation,
           clockid_t clock_id,
           wl_surface* wl_surface,
           uint32_t time,
           FeedbackObserver* observer = nullptr);

  ~Feedback();

  /// Called by Window after constructing each Feedback to register a hook
  /// that removes the entry from presentation_.feedback_list once the
  /// compositor sends presented or discarded.  The hook is invoked with
  /// `this` before the Wayland object is destroyed.
  void set_on_done(std::function<void(Feedback*)> cb) {
    on_done_ = std::move(cb);
  }

 private:
  static std::atomic<unsigned> sequence_;

  wl_proxy* wp_presentation_;
  clockid_t clock_id_ = -1;
  wl_proxy* feedback_;
  FeedbackObserver* observer_;
  std::function<void(Feedback*)> on_done_;

  timespec committed_{};
  timespec presented_{};
  uint32_t frame_stamp_{};
  unsigned frame_no_{};

  static void handle_sync_output(
      void* data,
      wl_proxy* /*wp_presentation_feedback*/,
      wl_output* output);

  static void handle_presented(
      void* data,
      wl_proxy* /*wp_presentation_feedback*/,
      uint32_t tv_sec_hi,
      uint32_t tv_sec_lo,
      uint32_t tv_nano_sec,
      uint32_t refresh,
      uint32_t seq_hi,
      uint32_t seq_lo,
      uint32_t flags);

  static void handle_discarded(
      void* data,
      wl_proxy* /*wp_presentation_feedback*/);

  static const void* listener_[] = {
      reinterpret_cast<const void*>(&handle_sync_output),
      reinterpret_cast<const void*>(&handle_presented),
      reinterpret_cast<const void*>(&handle_discarded),
  };
};