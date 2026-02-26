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

#include "waypp/seat/keyboard.h"

#include <cstring>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "logging/logging.h"

// workaround for Wayland macro not compiling in C++
#define WL_ARRAY_FOR_EACH(pos, array, type)                               \
  for ((pos) = (type)(array)->data;                                       \
       (const char*)(pos) < ((const char*)(array)->data + (array)->size); \
       (pos)++)

/**
 * @class Keyboard
 * @brief Represents a keyboard device
 *
 * The Keyboard class provides a wrapper for a keyboard device,
 * which interacts with the Wayland compositor.
 */
Keyboard::Keyboard(wl_keyboard* keyboard, const event_mask& event_mask)
    : wl_keyboard_(keyboard),
      xkb_context_(xkb_context_new(XKB_CONTEXT_NO_FLAGS)),
      event_mask_({
          .enabled = event_mask.enabled,
          .all = event_mask.all,
      }) {
  DLOG_DEBUG("Keyboard");

  // Open the self-pipe used by the signal handler to wake the GLib event loop.
  // Both ends are non-blocking, so the signal handler never blocks, and
  // O_CLOEXEC ensures the fds are not leaked into child processes.
  int pipefd[2];
  if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == 0) {
    repeat_.pipe_read_fd = pipefd[0];
    repeat_.pipe_write_fd = pipefd[1];
  } else {
    LOG_ERROR("[Keyboard] pipe2 failed: {}", std::strerror(errno));
  }

  wl_keyboard_add_listener(wl_keyboard_, &keyboard_listener_, this);
}

/**
 * @class Keyboard
 * @brief Represents a keyboard input device.
 *
 * The Keyboard class manages the interaction with a Wayland keyboard input
 * device.
 */
Keyboard::~Keyboard() {
  // Remove the GLib IO watch first, so no callback fires after destruction.
  if (repeat_.io_watch_id) {
    g_source_remove(repeat_.io_watch_id);
    repeat_.io_watch_id = 0;
  }

  if (repeat_.timer) {
    constexpr itimerspec its{};
    timer_settime(repeat_.timer, 0, &its, nullptr);
    timer_delete(repeat_.timer);
  }

  // Close the self-pipe.
  if (repeat_.pipe_read_fd >= 0) {
    close(repeat_.pipe_read_fd);
    repeat_.pipe_read_fd = -1;
  }
  if (repeat_.pipe_write_fd >= 0) {
    close(repeat_.pipe_write_fd);
    repeat_.pipe_write_fd = -1;
  }

  wl_keyboard_release(wl_keyboard_);

  if (xkb_state_) {
    xkb_state_unref(xkb_state_);
  }
  if (xkb_keymap_) {
    xkb_keymap_unref(xkb_keymap_);
  }
  if (xkb_context_) {
    xkb_context_unref(xkb_context_);
  }
}

void Keyboard::handle_keymap(void* data,
                             wl_keyboard* wl_keyboard,
                             uint32_t format,
                             int fd,
                             uint32_t size) {
  const auto obj = static_cast<Keyboard*>(data);
  if (obj->wl_keyboard_ != wl_keyboard) {
    return;
  }

  obj->format_ = static_cast<wl_keyboard_keymap_format>(format);

  if (obj->format_ == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    /// From version 7 onwards, the fd must be mapped with MAP_PRIVATE by the
    /// recipient, as MAP_SHARED may fail.
    constexpr int prot = PROT_READ;
    const int flags =
        (wl_keyboard_get_version(wl_keyboard) >= 7) ? MAP_PRIVATE : MAP_SHARED;
    const auto keymap_string =
        static_cast<char*>(mmap(nullptr, size, prot, flags, fd, 0));

    if (keymap_string == MAP_FAILED) {
      LOG_ERROR(
          "[Keyboard] mmap of keymap fd failed ({}): {} — "
          "keymap and key state unchanged",
          errno, std::strerror(errno));
      close(fd);
      return;
    }

    xkb_keymap* new_keymap = xkb_keymap_new_from_string(
        obj->xkb_context_, keymap_string, XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(keymap_string, size);

    if (!new_keymap) {
      LOG_ERROR(
          "[Keyboard] xkb_keymap_new_from_string failed — "
          "keymap and key state unchanged");
      close(fd);
      return;
    }

    // Replace the keymap first, then rebuild state from the new keymap.
    xkb_keymap_unref(obj->xkb_keymap_);
    obj->xkb_keymap_ = new_keymap;

    xkb_state* new_state = xkb_state_new(obj->xkb_keymap_);
    if (!new_state) {
      LOG_ERROR("[Keyboard] xkb_state_new failed — key state cleared");
    }
    xkb_state_unref(obj->xkb_state_);
    obj->xkb_state_ = new_state;  // maybe nullptr; handle_key guards this
  } else {
    LOG_WARN("Usage without libxkbcommon is currently not supported.");
  }

  // Notify observers before closing the fd so they can mmap it themselves
  // if needed (the Wayland protocol transfers fd ownership to the client).
  for (const auto& observer : obj->observers_) {
    observer->notify_keyboard_keymap(obj, wl_keyboard, format, fd, size);
  }
  close(fd);
}

void Keyboard::handle_enter(void* data,
                            wl_keyboard* wl_keyboard,
                            uint32_t serial,
                            struct wl_surface* wl_surface,
                            wl_array* keys) {
  const auto obj = static_cast<Keyboard*>(data);
  if (obj->wl_keyboard_ != wl_keyboard) {
    return;
  }

  DLOG_TRACE("[Keyboard] handle_enter");

  obj->wl_surface = wl_surface;

  if (obj->event_mask_.enabled && obj->event_mask_.all) {
    return;
  }

  if (obj->format_ == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    if (keys->size) {
      const uint32_t* key;
      WL_ARRAY_FOR_EACH(key, keys, const uint32_t*) {
        handle_key(data, wl_keyboard, serial, 0, *key,
                   WL_KEYBOARD_KEY_STATE_PRESSED);
      }
    }
  }

  for (const auto observer : obj->observers_) {
    observer->notify_keyboard_enter(obj, wl_keyboard, serial, wl_surface, keys);
  }
}

void Keyboard::handle_leave(void* data,
                            wl_keyboard* wl_keyboard,
                            uint32_t serial,
                            struct wl_surface* wl_surface) {
  const auto obj = static_cast<Keyboard*>(data);
  if (obj->wl_keyboard_ != wl_keyboard) {
    return;
  }

  DLOG_TRACE("[Keyboard] handle_leave");

  obj->wl_surface = nullptr;

  if (obj->event_mask_.enabled && obj->event_mask_.all) {
    return;
  }

  for (const auto observer : obj->observers_) {
    observer->notify_keyboard_leave(obj, wl_keyboard, serial, wl_surface);
  }
}

void Keyboard::handle_key(void* data,
                          wl_keyboard* wl_keyboard,
                          uint32_t serial,
                          uint32_t time,
                          uint32_t key,
                          uint32_t state) {
  const auto obj = static_cast<Keyboard*>(data);
  if (obj->wl_keyboard_ != wl_keyboard) {
    return;
  }

  if (!obj->xkb_state_)
    return;

  if (obj->format_ == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    /// translate scancode to XKB scancode
    const auto xkb_scancode = key + 8;
    const auto key_repeats =
        xkb_keymap_key_repeats(obj->xkb_keymap_, xkb_scancode);

    const xkb_keysym_t* key_syms;
    const auto xdg_keysym_count =
        xkb_state_key_get_syms(obj->xkb_state_, xkb_scancode, &key_syms);

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      if (key_repeats && !obj->repeat_setup_failed_) {
        // start/restart timer
        itimerspec in{};
        in.it_value.tv_nsec = obj->repeat_.delay * 1000000;
        in.it_interval.tv_nsec = obj->repeat_.rate * 1000000;
        timer_settime(obj->repeat_.timer, 0, &in, nullptr);

        // update notify values
        obj->repeat_.notify = {
            .wl_keyboard = wl_keyboard,
            .serial = serial,
            .time = time,
            .xkb_scancode = xkb_scancode,
            .key_repeats = key_repeats,
            .xdg_keysym_count = xdg_keysym_count,
            .key_syms = key_syms,
        };
      }

    } else if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
      if (!obj->repeat_setup_failed_ &&
          obj->repeat_.notify.xkb_scancode == xkb_scancode) {
        // stop timer
        itimerspec its{};
        timer_settime(obj->repeat_.timer, 0, &its, nullptr);
      }
    }

    if (obj->event_mask_.enabled && obj->event_mask_.all) {
      return;
    }

    for (const auto observer : obj->observers_) {
      observer->notify_keyboard_xkb_v1_key(obj, wl_keyboard, serial, time,
                                           xkb_scancode, key_repeats, state,
                                           xdg_keysym_count, key_syms);
    }
  }
}

void Keyboard::handle_modifiers(void* data,
                                wl_keyboard* keyboard,
                                uint32_t /* serial */,
                                uint32_t mods_depressed,
                                uint32_t mods_latched,
                                uint32_t mods_locked,
                                uint32_t group) {
  const auto obj = static_cast<Keyboard*>(data);
  if (obj->wl_keyboard_ != keyboard) {
    return;
  }

  DLOG_TRACE("[Keyboard] handle_modifiers");

  if (obj->format_ == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    if (!obj->xkb_state_)
      return;
    xkb_state_update_mask(obj->xkb_state_, mods_depressed, mods_latched,
                          mods_locked, 0, 0, group);
  }
}

void Keyboard::handle_repeat_info(void* data,
                                  wl_keyboard* keyboard,
                                  int32_t rate,
                                  int32_t delay) {
  const auto obj = static_cast<Keyboard*>(data);
  if (obj->wl_keyboard_ != keyboard) {
    return;
  }

  DLOG_TRACE("[Keyboard] handle_repeat_info: rate: {}, delay: {}", rate, delay);

  obj->repeat_.rate = rate;
  obj->repeat_.delay = delay;

  if (obj->format_ == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    if (!obj->repeat_.timer) {
      /// Setup signal event – the signal handler only writes to the self-pipe;
      /// the GLib IO watch (repeat_dispatch_cb) does the actual observer
      /// notification on the main event-loop thread.
      obj->repeat_.sev.sigev_notify = SIGEV_SIGNAL;
      obj->repeat_.sev.sigev_signo = SIGRTMIN;
      obj->repeat_.sev.sigev_value.sival_ptr = data;
      const auto res =
          timer_create(CLOCK_REALTIME, &obj->repeat_.sev, &obj->repeat_.timer);
      if (res != 0) {
        LOG_ERROR(
            "[Keyboard] timer_create failed ({}): {} — key-repeat disabled",
            errno, std::strerror(errno));
        obj->repeat_setup_failed_ = true;
        return;
      }

      /// Setup signal action – handler is minimal and async-signal-safe.
      obj->repeat_.sa.sa_flags = SA_SIGINFO;
      obj->repeat_.sa.sa_sigaction = repeat_xkb_v1_key_callback;
      sigemptyset(&obj->repeat_.sa.sa_mask);
      if (sigaction(SIGRTMIN, &obj->repeat_.sa, nullptr) == -1) {
        LOG_ERROR("[Keyboard] sigaction failed ({}): {} — key-repeat disabled",
                  errno, std::strerror(errno));
        // The timer was created successfully; destroy it before marking
        // invalid.
        timer_delete(obj->repeat_.timer);
        obj->repeat_.timer = {};
        obj->repeat_setup_failed_ = true;
        return;
      }

      /// Attach a GLib IO watch on the read end of the self-pipe.
      /// repeat_dispatch_cb() runs on the main thread and is free to call
      /// arbitrary C++ (virtual dispatch, std::list iteration, etc.).
      if (obj->repeat_.pipe_read_fd >= 0) {
        GIOChannel* channel = g_io_channel_unix_new(obj->repeat_.pipe_read_fd);
        // Raw binary I/O – do not interpret the single-byte token.
        g_io_channel_set_encoding(channel, nullptr, nullptr);
        g_io_channel_set_buffered(channel, FALSE);
        // g_io_add_watch_full accepts a GIOFunc directly (no cast needed) and
        // returns a source ID that can be used for cleanup.
        obj->repeat_.io_watch_id =
            g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, G_IO_IN,
                                repeat_dispatch_cb, data, nullptr);
        g_io_channel_unref(channel);
      } else {
        LOG_ERROR(
            "[Keyboard] self-pipe not available; key-repeat will not work");
      }
    }
  }
}

const wl_keyboard_listener Keyboard::keyboard_listener_ = {
    .keymap = handle_keymap,
    .enter = handle_enter,
    .leave = handle_leave,
    .key = handle_key,
    .modifiers = handle_modifiers,
    .repeat_info = handle_repeat_info,
};

/**
 * @brief Async-signal-safe key-repeat signal handler.
 *
 * POSIX restricts what may be called from a signal handler to a small set of
 * async-signal-safe functions. C++ virtual dispatch, std::list iteration, and
 * heap allocation are NOT in that set and must not be called here.
 *
 * This handler only:
 *   1. Sets an atomic flag (lock-free store – async-signal-safe).
 *   2. Writes one byte to the self-pipe to wake the GLib event loop.
 *
 * All observer notifications are deferred to repeat_dispatch_cb(), which runs
 * on the normal event-loop thread via the GLib IO watch.
 */
void Keyboard::repeat_xkb_v1_key_callback(int /* sig */,
                                          siginfo_t* si,
                                          void* /* uc */) {
  auto* obj = static_cast<Keyboard*>(si->_sifields._rt.si_sigval.sival_ptr);

  // Mark a repeat as pending (relaxed store is sufficient – the IO watch read
  // on the other end of the pipe provides the necessary memory ordering).
  obj->repeat_.pending.store(true, std::memory_order_relaxed);

  // Write one byte token.  O_NONBLOCK ensures this never blocks in a signal
  // handler.  EINTR / EAGAIN are silently ignored; if the pipe is full, the
  // pending flag is still set and the existing byte will be drained.
  constexpr char token = 1;
  const ssize_t n = write(obj->repeat_.pipe_write_fd, &token, 1);
  static_cast<void>(n);  // intentionally ignored — see the comment above
}

/**
 * @brief GLib IO watch callback – dispatches key-repeat observer notifications.
 *
 * This function runs on the main event-loop thread (not in signal context), so
 * it is free to call arbitrary C++: virtual methods, std::list traversal, heap
 * allocation, and logging.
 *
 * It drains all pending bytes from the self-pipe then fires the observer chain
 * once per pending batch.
 */
gboolean Keyboard::repeat_dispatch_cb(GIOChannel* /* channel */,
                                      GIOCondition condition,
                                      gpointer user_data) {
  if (!(condition & G_IO_IN)) {
    return G_SOURCE_CONTINUE;
  }

  auto* obj = static_cast<Keyboard*>(user_data);

  // Drain the pipe – each byte corresponds to one (or more coalesced) signals.
  char buf[64];
  while (read(obj->repeat_.pipe_read_fd, buf, sizeof(buf)) > 0) {
  }

  // Clear the pending flag *after* draining so we don't miss a signal that
  // arrived between the last read() and this store.
  obj->repeat_.pending.store(false, std::memory_order_relaxed);

  if (obj->event_mask_.enabled && obj->event_mask_.all) {
    return G_SOURCE_CONTINUE;
  }

  for (const auto observer : obj->observers_) {
    observer->notify_keyboard_xkb_v1_key(
        obj, obj->repeat_.notify.wl_keyboard, obj->repeat_.notify.serial,
        obj->repeat_.notify.time, obj->repeat_.notify.xkb_scancode,
        obj->repeat_.notify.key_repeats, WL_KEYBOARD_KEY_STATE_PRESSED,
        obj->repeat_.notify.xdg_keysym_count, obj->repeat_.notify.key_syms);
  }

  return G_SOURCE_CONTINUE;
}

void Keyboard::set_event_mask(const event_mask& event_mask) {
  event_mask_.enabled = event_mask.enabled;
  event_mask_.all = event_mask.all;
}