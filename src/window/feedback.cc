
#include "feedback.h"

#include <chrono>

unsigned Feedback::sequence_ = 0;

Feedback::Feedback(struct wp_presentation* wp_presentation,
                   clockid_t clock_id,
                   struct wl_surface* wl_surface,
                   uint32_t time,
                   FeedbackObserver* observer)
    : wp_presentation_(wp_presentation),
      clock_id_(clock_id),
      observer_(observer) {
  clock_gettime(clock_id_, &committed_);
  frame_stamp_ = time;
  frame_no_ = ++Feedback::sequence_;

  feedback_ = wp_presentation_feedback(wp_presentation, wl_surface);
  wp_presentation_feedback_add_listener(feedback_, &listener_, this);
}

Feedback::~Feedback() {
  if (feedback_) {
    wp_presentation_feedback_destroy(feedback_);
  }
}

void Feedback::handle_sync_output(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    struct wl_output* output) {
  auto f = static_cast<Feedback*>(data);
  if (f->feedback_ != wp_presentation_feedback) {
    return;
  }

  if (f->observer_) {
    f->observer_->notify_feedback_sync_output(f, wp_presentation_feedback,
                                              output);
  }
}

void Feedback::handle_presented(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    uint32_t tv_sec_hi,
    uint32_t tv_sec_lo,
    uint32_t tv_nano_sec,
    uint32_t refresh,
    uint32_t seq_hi,
    uint32_t seq_lo,
    uint32_t flags) {
  auto f = static_cast<Feedback*>(data);
  if (f->feedback_ != wp_presentation_feedback) {
    return;
  }

  clock_gettime(f->clock_id_, &f->presented_);

  if (f->observer_) {
    f->observer_->notify_feedback_presented(
        f, wp_presentation_feedback, tv_sec_hi, tv_sec_lo, tv_nano_sec, refresh,
        seq_hi, seq_lo, flags, f->committed_, f->presented_, f->frame_stamp_,
        f->frame_no_);
  }
}

void Feedback::handle_discarded(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback) {
  auto f = static_cast<Feedback*>(data);
  if (f->feedback_ != wp_presentation_feedback) {
    return;
  }

  if (f->observer_) {
    f->observer_->notify_feedback_discarded(f, wp_presentation_feedback);
  }
}
