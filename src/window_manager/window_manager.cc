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

#include "waypp/window_manager/window_manager.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <wayland-client.h>
#include <cerrno>

#include "logging/logging.h"
#include "waypp/seat/keyboard.h"
#include "waypp/seat/seat.h"
#include "waypp/window_manager/registrar.h"

class Registrar;

WindowManager::WindowManager(wl_display* display,
                             const bool disable_cursor,
                             const unsigned long ext_interface_count,
                             const RegistrarCallback* ext_interface_data,
                             GMainContext* context)
    : Registrar(display,
                ext_interface_count,
                ext_interface_data,
                disable_cursor),
      outputs_(get_outputs()) {
  (void)context;
  DLOG_TRACE("++WindowManager::WindowManager()");
  int pfd[2];
  if (pipe2(pfd, O_CLOEXEC | O_NONBLOCK) == 0) {
    wake_pipe_read_fd_ = pfd[0];
    wake_pipe_write_fd_ = pfd[1];
  } else {
    LOG_ERROR("WindowManager: pipe2 failed: {}", std::strerror(errno));
  }
  DLOG_TRACE("--WindowManager::WindowManager()");
}

WindowManager::~WindowManager() {
  DLOG_TRACE("++WindowManager::~WindowManager()");
  stop_compositor_thread();
  if (wake_pipe_read_fd_ >= 0)
    close(wake_pipe_read_fd_);
  if (wake_pipe_write_fd_ >= 0)
    close(wake_pipe_write_fd_);
  DLOG_TRACE("--WindowManager::~WindowManager()");
}

// ─────────────────────────────────────────────────────────────────────────────
// Compositor thread
//
// This thread is the sole owner of all wl_display I/O.  Nothing outside this
// thread may call wl_display_dispatch*, wl_display_prepare_read*,
// wl_display_read_events, or wl_display_cancel_read.
//
// Design:
//   1. wl_display_flush() — push any queued outgoing requests.
//   2. wl_display_prepare_read() — announce intent to read.  Because this
//      thread is the only reader, this always succeeds immediately.
//   3. ppoll([wayland_fd, pipe_fd], -1) — block until data arrives on either.
//   4a. If wayland_fd is readable: wl_display_read_events() releases the read
//       lock and fills the queue, then wl_display_dispatch_pending() fires
//       callbacks.  Callbacks may safely call wl_display_flush() because the
//       read lock is already released.
//   4b. If only pipe_fd: cancel_read() then drain + dispatch_repeat().
//   5. Repeat.
// ─────────────────────────────────────────────────────────────────────────────

void WindowManager::compositor_thread_func() {
  DLOG_DEBUG("compositor thread: start");

  while (!compositor_stop_.load(std::memory_order_acquire)) {
    // ── Flush outgoing requests ─────────────────────────────────────────
    const int flush_ret = wl_display_flush(get_display());
    if (flush_ret < 0 && errno != EAGAIN) {
      LOG_ERROR("compositor thread: wl_display_flush error: {}",
                std::strerror(errno));
      break;
    }

    // ── Acquire read lock ───────────────────────────────────────────────
    // We are the sole reader — prepare_read always returns 0 immediately.
    if (wl_display_prepare_read(get_display()) != 0) {
      // Queue already has events (e.g., from a callback that enqueued more).
      wl_display_dispatch_pending(get_display());
      continue;
    }

    // ── Build ppoll fd set ──────────────────────────────────────────────
    Keyboard* kb = nullptr;
    if (const auto seat_opt = get_seat()) {
      if (const auto kb_opt = (*seat_opt)->get_keyboard()) {
        kb = *kb_opt;
      }
    }

    const int pipe_fd = kb ? kb->get_pipe_read_fd() : -1;

    // Slots: 0=wayland, 1=wake(stop), 2=key-repeat(optional)
    pollfd fds[3]{};
    int nfds = 0;
    fds[nfds++] = {wl_display_get_fd(get_display()), POLLIN, 0};
    const int wake_slot = nfds;
    if (wake_pipe_read_fd_ >= 0)
      fds[nfds++] = {wake_pipe_read_fd_, POLLIN, 0};
    const int kb_slot = nfds;
    if (pipe_fd >= 0)
      fds[nfds++] = {pipe_fd, POLLIN, 0};

    // If the last flush hit EAGAIN, also watch for writability.
    if (flush_ret < 0 && errno == EAGAIN)
      fds[0].events |= POLLOUT;

    // ── Wait ────────────────────────────────────────────────────────────

    if (const int ret = poll(fds, static_cast<nfds_t>(nfds), -1); ret < 0) {
      if (errno == EINTR) {
        wl_display_cancel_read(get_display());
        continue;
      }
      LOG_ERROR("compositor thread: poll error: {}", std::strerror(errno));
      wl_display_cancel_read(get_display());
      break;
    }

    // ── Stop wake-pipe ──────────────────────────────────────────────────
    if (wake_pipe_read_fd_ >= 0 && (fds[wake_slot].revents & POLLIN)) {
      char buf[64];
      while (::read(wake_pipe_read_fd_, buf, sizeof(buf)) > 0) {
      }
      wl_display_cancel_read(get_display());
      break;
    }

    // ── Wayland fd error ────────────────────────────────────────────────
    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
      LOG_ERROR("compositor thread: Wayland fd error/hangup");
      wl_display_cancel_read(get_display());
      break;
    }

    // ── Retry flush if the socket became writable ───────────────────────────
    if (fds[0].revents & POLLOUT)
      wl_display_flush(get_display());

    // ── Read + dispatch Wayland events ──────────────────────────────────
    if (fds[0].revents & POLLIN) {
      if (wl_display_read_events(get_display()) == -1) {
        LOG_ERROR("compositor thread: wl_display_read_events error: {}",
                  std::strerror(errno));
        break;
      }
      if (wl_display_dispatch_pending(get_display()) < 0) {
        LOG_ERROR("compositor thread: dispatch_pending error: {}",
                  std::strerror(errno));
        break;
      }
    } else {
      wl_display_cancel_read(get_display());
    }

    // ── Key-repeat pipe ─────────────────────────────────────────────────
    if (kb && pipe_fd >= 0 && (fds[kb_slot].revents & POLLIN)) {
      char buf[64];
      while (::read(pipe_fd, buf, sizeof(buf)) > 0) {
      }
      kb->dispatch_repeat();
    }
  }

  DLOG_DEBUG("compositor thread: stop");
}

void WindowManager::start_compositor_thread() {
  if (compositor_thread_.joinable())
    return;
  compositor_stop_.store(false, std::memory_order_release);
  compositor_thread_ =
      std::thread(&WindowManager::compositor_thread_func, this);
}

void WindowManager::stop_compositor_thread() {
  if (!compositor_thread_.joinable())
    return;
  compositor_stop_.store(true, std::memory_order_release);
  // Wake the compositor thread's poll() by writing one byte to the wake pipe.
  // This is the only safe cross-thread wakeup — no Wayland calls are made.
  if (wake_pipe_write_fd_ >= 0) {
    constexpr char token = 1;
    if (write(wake_pipe_write_fd_, &token, 1) < 0) {
      LOG_ERROR("WindowManager: wake pipe write failed: {}",
                std::strerror(errno));
    }
  }
  compositor_thread_.join();
}

// ─────────────────────────────────────────────────────────────────────────────
// Pre-thread blocking dispatch — used by window.cc before
// start_compositor_thread
// ─────────────────────────────────────────────────────────────────────────────

int WindowManager::display_dispatch() const {
  // Single blocking roundtrip — only valid before the compositor thread starts.
  if (wl_display_prepare_read(get_display()) != 0)
    return wl_display_dispatch_pending(get_display());

  wl_display_flush(get_display());

  pollfd pfd{wl_display_get_fd(get_display()), POLLIN, 0};
  if (poll(&pfd, 1, -1) > 0 && (pfd.revents & POLLIN)) {
    wl_display_read_events(get_display());
    return wl_display_dispatch_pending(get_display());
  }

  wl_display_cancel_read(get_display());
  return 0;
}

[[maybe_unused]] int WindowManager::dispatch(const int timeout) const {
  // Legacy shim — used by window.cc for bounded roundtrips before the
  // compositor thread starts.  Same single-owner-thread assumptions as
  // display_dispatch().
  if (wl_display_prepare_read(get_display()) != 0)
    return wl_display_dispatch_pending(get_display());

  wl_display_flush(get_display());

  pollfd pfd{wl_display_get_fd(get_display()), POLLIN, 0};
  const int ret = poll(&pfd, 1, timeout);
  if (ret > 0 && (pfd.revents & POLLIN)) {
    wl_display_read_events(get_display());
    return wl_display_dispatch_pending(get_display());
  }

  wl_display_cancel_read(get_display());
  if (ret < 0) {
    const int saved = errno;
    return (saved == EINTR) ? 0 : -saved;
  }
  return 0;
}

int WindowManager::poll_events(const int timeout) const {
  for (const auto observer : observers_)
    observer->notify_task();

  if (wl_display_prepare_read(get_display()) != 0)
    return wl_display_dispatch_pending(get_display());

  wl_display_flush(get_display());

  pollfd pfd{wl_display_get_fd(get_display()), POLLIN, 0};
  if (poll(&pfd, 1, timeout) > 0 && (pfd.revents & POLLIN)) {
    wl_display_read_events(get_display());
    return wl_display_dispatch_pending(get_display());
  }

  wl_display_cancel_read(get_display());
  return wl_display_dispatch_pending(get_display());
}

// ─────────────────────────────────────────────────────────────────────────────
// Output helpers
// ─────────────────────────────────────────────────────────────────────────────

wl_output* WindowManager::get_primary_output() const {
  auto& outputs = get_outputs();
  if (get_xdg_output_manager()) {
    for (const auto& [fst, snd] : outputs) {
      if (snd->get_xdg_output()->is_origin()) {
        LOG_DEBUG("get_primary_output: (xdg_output) Origin: {}", fmt::ptr(fst));
        return fst;
      }
    }
  } else {
    for (const auto& [fst, snd] : outputs) {
      LOG_DEBUG("get_primary_output: (first) Origin: {}", fmt::ptr(fst));
      return fst;
    }
  }
  LOG_DEBUG("get_primary_output: (nullptr)");
  return nullptr;
}

wl_output* WindowManager::find_output_by_name(
    const std::string& output_name) const {
  auto& outputs = get_outputs();
  if (get_xdg_output_manager()) {
    for (const auto& [fst, snd] : outputs) {
      if (snd->get_name() == output_name) {
        LOG_DEBUG("find_output_by_name: (xdg_output): {}", output_name);
        return fst;
      }
    }
  } else {
    for (const auto& [fst, snd] : outputs) {
      LOG_DEBUG("find_output_by_name: (first): {}", output_name);
      return fst;
    }
  }
  return nullptr;
}
