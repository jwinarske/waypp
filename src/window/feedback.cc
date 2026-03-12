
#include "waypp/window/feedback.h"

#include <chrono>

#include "logging/logging.h"

std::atomic<unsigned> Feedback::sequence_{0};

Feedback::Feedback(wl_proxy* wp_presentation,
                   const clockid_t clock_id,
                   wl_surface* wl_surface,
                   const uint32_t time,
                   FeedbackObserver* observer)
    : wp_presentation_(wp_presentation),
      clock_id_(clock_id),
      observer_(observer) {
  clock_gettime(clock_id_, &committed_);
  frame_stamp_ = time;
  frame_no_ = sequence_.fetch_add(1u, std::memory_order_relaxed) + 1u;

  feedback_ = wl_proxy_marshal_constructor(
      wp_presentation,
      presentation_time::client::wp_presentation_traits::Op::Feedback,
      &presentation_time::client::wp_presentation_feedback_traits::wl_iface(),
      nullptr, (wl_proxy*)wl_surface);
  wl_proxy_add_listener(feedback_,
                        reinterpret_cast<void(**)(void)>(
                            const_cast<void**>(listener_)),
                        this);

  (void)wp_presentation_;  // suppress unused warning
}

Feedback::~Feedback() {
  if (feedback_) {
    DLOG_TRACE("[Feedback] wp_presentation_feedback_destroy(feedback_)");
    wl_proxy_destroy(feedback_);
  }
}

void Feedback::handle_sync_output(
    void* data,
    wl_proxy* wp_presentation_feedback,
    wl_output* output) {
  const auto f = static_cast<Feedback*>(data);

  if (f->observer_) {
    f->observer_->notify_feedback_sync_output(f, wp_presentation_feedback,
                                              output);
  }
}

void Feedback::handle_presented(
    void* data,
    wl_proxy* /*wp_presentation_feedback*/,
    const uint32_t tv_sec_hi,
    const uint32_t tv_sec_lo,
    const uint32_t tv_nano_sec,
    const uint32_t refresh,
    const uint32_t seq_hi,
    const uint32_t seq_lo,
    const uint32_t flags) {
  const auto f = static_cast<Feedback*>(data);

  clock_gettime(f->clock_id_, &f->presented_);

  if (f->observer_) {
    f->observer_->notify_feedback_presented(
        f, wp_presentation_feedback, tv_sec_hi, tv_sec_lo, tv_nano_sec, refresh,
        seq_hi, seq_lo, flags, f->committed_, f->presented_, f->frame_stamp_,
        f->frame_no_);
  }

  if (f->on_done_) {
    f->on_done_(f);
  }
}

void Feedback::handle_discarded(
    void* data,
    wl_proxy* /*wp_presentation_feedback*/) {
  const auto f = static_cast<Feedback*>(data);

  if (f->observer_) {
    f->observer_->notify_feedback_discarded(f, nullptr);
  }

  if (f->on_done_) {
    f->on_done_(f);
  }
}
