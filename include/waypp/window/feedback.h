#pragma once

#include <waypp/waypp.h>

class Feedback;

class FeedbackObserver {
 public:
  virtual ~FeedbackObserver() = default;

  virtual void notify_feedback_sync_output(
      Feedback* feedback,
      struct wp_presentation_feedback* wp_presentation_feedback,
      wl_output* output) = 0;

  virtual void notify_feedback_presented(
      Feedback* feedback,
      struct wp_presentation_feedback* wp_presentation_feedback,
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
      struct wp_presentation_feedback* wp_presentation_feedback) = 0;
};

class Feedback {
 public:
  Feedback(wp_presentation* wp_presentation,
           clockid_t clock_id,
           wl_surface* wl_surface,
           uint32_t time,
           FeedbackObserver* observer = nullptr);

  ~Feedback();

 private:
  static unsigned sequence_;

  [[maybe_unused]] wp_presentation* wp_presentation_;
  clockid_t clock_id_ = -1;
  struct wp_presentation_feedback* feedback_;
  FeedbackObserver* observer_;

  timespec committed_{};
  timespec presented_{};
  uint32_t frame_stamp_{};
  unsigned frame_no_{};

  static void handle_sync_output(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback,
      wl_output* output);

  static void handle_presented(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback,
      uint32_t tv_sec_hi,
      uint32_t tv_sec_lo,
      uint32_t tv_nano_sec,
      uint32_t refresh,
      uint32_t seq_hi,
      uint32_t seq_lo,
      uint32_t flags);

  static void handle_discarded(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback);

  static constexpr wp_presentation_feedback_listener listener_ = {
      .sync_output = handle_sync_output,
      .presented = handle_presented,
      .discarded = handle_discarded,
  };
};