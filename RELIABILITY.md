# waypp Reliability Analysis

**Date:** 2026-02-25  
**Scope:** `examples/`, `include/`, `src/`

---

## Table of Contents

1. [Summary](#summary)
2. [Critical Issues](#critical-issues)
3. [High Severity Issues](#high-severity-issues)
4. [Medium Severity Issues](#medium-severity-issues)
5. [Low Severity / Code Quality Issues](#low-severity--code-quality-issues)
6. [Per-File Findings](#per-file-findings)
7. [Positive Patterns](#positive-patterns)

---

## Summary

| Severity | Count | Open |
|----------|-------|------|
| Critical | 4 | 0 |
| High     | 8 | 0 |
| Medium   | 9 | 0 |
| Low      | 9 | 0 |
| **Total**| **30** | **0** |

---

## Critical Issues

### CRIT-1 — Signal handler invokes async-signal-unsafe observer chain ✅ FIXED  
**File:** `src/seat/keyboard.cc` — `repeat_xkb_v1_key_callback()`  
**Lines:** 311–325

```cpp
void Keyboard::repeat_xkb_v1_key_callback(int, siginfo_t* si, void*) {
  const auto obj = static_cast<Keyboard*>(si->_sifields._rt.si_sigval.sival_ptr);
  for (const auto observer : obj->observers_) {
    observer->notify_keyboard_xkb_v1_key(...);
  }
}
```

**Problem:** The real-time signal handler (SIGRTMIN) iterated over `std::list<KeyboardObserver*>` and called virtual methods on each observer. None of these operations are async-signal-safe. This can deadlock the process if the signal fires while a mutex is held elsewhere, corrupt heap state, or produce undefined behaviour per POSIX.  

**Fix applied (2025-02-25):**  
- A **self-pipe** (`pipe2(O_CLOEXEC | O_NONBLOCK)`) is opened in the `Keyboard` constructor and closed in the destructor.  
- The signal handler (`repeat_xkb_v1_key_callback`) now **only** sets `repeat_.pending` (a `std::atomic<bool>`, lock-free) and writes one byte to the write end of the pipe — both async-signal-safe operations.  
- A **GLib IO watch** (`g_io_create_watch` + `g_source_attach`) is registered on the read end of the pipe in `handle_repeat_info`. The watch callback (`repeat_dispatch_cb`) runs on the main event-loop thread, drains the pipe, and fires the full observer chain. All C++ virtual dispatch and `std::list` iteration now happen exclusively off-signal-stack.  
- The GLib IO source is destroyed (`g_source_destroy` / `g_source_unref`) before the timer is stopped in the destructor, preventing a use-after-free if a signal fires during teardown.

---

### CRIT-2 — `abort()` called on timer/sigaction setup failure ✅ FIXED  
**File:** `src/seat/keyboard.cc` — `handle_repeat_info()`  
**Lines:** 290–305

```cpp
const auto res = timer_create(CLOCK_REALTIME, &obj->repeat_.sev, &obj->repeat_.timer);
if (res != 0) {
    LOG_CRITICAL("Error timer_create: {}", std::strerror(errno));
    abort();
}
if (sigaction(SIGRTMIN, &obj->repeat_.sa, nullptr) == -1) {
    LOG_CRITICAL("Error sigaction: {}", std::strerror(errno));
    abort();
}
```

**Problem:** `abort()` terminated the entire process (with core dump) when `timer_create` or `sigaction` failed, even though these failures are potentially recoverable (e.g. RLIMIT_SIGPENDING, inherited signal mask). There was no way for the caller to handle or observe the failure.

**Fix applied (2025-02-25):**
- Added `bool repeat_setup_failed_{false}` private member to `Keyboard`.
- Added `[[nodiscard]] bool is_repeat_valid() const` public accessor so callers can check whether key-repeat is available.
- On `timer_create` failure: logs at ERROR level (not CRITICAL), sets `repeat_setup_failed_ = true`, and returns. No `abort()`.
- On `sigaction` failure: additionally calls `timer_delete` on the already-created timer before setting `repeat_setup_failed_ = true` and returning — preventing a timer handle leak. No `abort()`.
- `handle_key` now guards all `timer_settime` calls (both arm and disarm) behind `!obj->repeat_setup_failed_`, so a null/zeroed `timer_t` handle is never passed to `timer_settime`.
- The GLib IO watch is still attached on the read end of the self-pipe in both success and failure paths; if setup fails, the timer is never created so the watch will simply never fire. The pipe and watch are still cleaned up in the destructor.
- The `Keyboard` object remains fully usable: `handle_enter`, `handle_leave`, `handle_key`, and `handle_modifiers` all continue to function — only key-repeat auto-fire is disabled.

---

### CRIT-3 — `exit()` called inside constructors ✅ FIXED  
**Files:**  
- `src/window_manager/xdg_window_manager.cc` — constructor  
- `src/window_manager/agl_shell.cc` — constructor (2 sites)  
- `src/window/xdg_toplevel.cc` — constructor  
- `examples/simple-shm.cc`, `examples/simple-egl.cc`, `examples/agl-simple-shm.cc` — App constructor / main()

**Problem:** Calling `exit()` inside a constructor bypasses all RAII cleanup for partially-constructed objects. Destructors for already-constructed sub-objects (Wayland handles, file descriptors, smart-pointer members) are not invoked via `exit()`, causing resource leaks. The pattern also made the API non-composable — callers had no way to recover from or even observe these failures.

**Fix applied (2026-02-25):**

*Library sources — replaced every `exit()` with `throw std::runtime_error`:*
- **`XdgWindowManager` constructor**: `exit(EXIT_FAILURE)` when `xdg_wm_base_` is null → `throw std::runtime_error("XDG Window Manager (xdg_wm_base) is not supported by the compositor")`. Added `#include <stdexcept>`.
- **`AglShell` constructor** (2 sites): `exit()` when `agl_shell_` is null and when `bound_ok_` is false → both replaced with `throw std::runtime_error(...)` carrying the original diagnostic message. Added `#include <stdexcept>`.
- **`XdgTopLevel` constructor**: `exit(EXIT_FAILURE)` when `xdg_wm_base` is null → `throw std::runtime_error("xdg_wm_base is not available; cannot create XdgTopLevel")`. Added `#include <stdexcept>`.

  With exceptions, the already-constructed base class (`Window`) destructor runs normally, releasing the `wl_surface_`, SHM buffers, EGL resources, etc.

*Examples — `wl_display_connect` failure and library construction wrapped safely:*
- **`simple-shm.cc`**: `exit()` on display connect → `throw std::runtime_error`. `main()` wraps `App` construction and run loop in `try/catch(const std::runtime_error&)` → logs and returns `EXIT_FAILURE`. Added `#include <stdexcept>`.
- **`agl-simple-shm.cc`**: same pattern as `simple-shm.cc`. Added `#include <stdexcept>`.
- **`simple-egl.cc`**: `exit()` on display connect → `return EXIT_FAILURE` (display not yet connected so no flush needed). `wm` and `toplevel_` construction moved inside `try` block; `catch` flushes and disconnects the display before returning `EXIT_FAILURE`. Added `#include <stdexcept>`.

---

### CRIT-4 — NULL keymap dereference after failed `xkb_keymap_new_from_string` ✅ FIXED  
**File:** `src/seat/keyboard.cc` — `handle_keymap()`

**Problem:** Three related defects in `handle_keymap`:

1. `mmap()` return value was never checked against `MAP_FAILED`. If `mmap` failed, the resulting invalid pointer was silently passed to `xkb_keymap_new_from_string`, causing undefined behaviour.
2. `xkb_keymap_new_from_string()` return value was never checked for `NULL`. If it returned `NULL` (e.g. malformed keymap data), the old `xkb_keymap_` was freed by `xkb_keymap_unref` and then `NULL` was stored. `xkb_state_new(NULL)` was then called — undefined behaviour — and the resulting invalid `xkb_state_` pointer was used by all subsequent `handle_key` and `handle_modifiers` calls.
3. `close(fd)` was called *before* `notify_keyboard_keymap` was dispatched to observers, so any observer that tried to re-map the fd (a valid use per the Wayland protocol, which transfers fd ownership to the client) would receive a closed fd.

**Fix applied (2026-02-25):**

- **`mmap` failure**: if `mmap` returns `MAP_FAILED`, log at ERROR level and `close(fd)` + `return` immediately. The existing `xkb_keymap_` and `xkb_state_` are left unchanged so key events continue to function with the previous keymap.
- **`xkb_keymap_new_from_string` failure**: the result is saved to `new_keymap` and checked before touching any member state. If `NULL`, log at ERROR level, `close(fd)` + `return`. Again, the existing keymap/state are left intact.
- **`xkb_state_new` failure**: even with a valid keymap, `xkb_state_new` can theoretically return `NULL` (OOM). The return value is now checked; on failure it is logged at ERROR level. The `nullptr` is stored in `xkb_state_` — `handle_key` already has a `if (!obj->xkb_state_) return;` guard (pre-existing) so this is safe.
- **Correct update order**: the old keymap is now freed and replaced *after* the new keymap is confirmed valid, preventing a window where `xkb_keymap_` is `NULL`.
- **`close(fd)` moved after observer notification**: observers receive the fd while it is still open, consistent with Wayland fd ownership semantics.

---

## High Severity Issues

### HIGH-1 — Duplicate event-mask check in `handle_leave` is dead code (logic error) ✅ FIXED  
**File:** `src/seat/keyboard.cc` — `handle_leave()`

**Problem:** `handle_leave` contained two identical `if (event_mask_.enabled && event_mask_.all) return;` guards. The first appeared *before* `obj->wl_surface = nullptr` — meaning when the mask was active the function returned without ever clearing the surface pointer. The second appeared *after* the clear and was entirely unreachable (the boolean cannot change between the two checks). As a result, the observer notification loop was never reached when `event_mask_.all` was true, and `wl_surface` was left non-null on masked leave events.

**Fix applied (2026-02-25):**  
Removed the first (misplaced) guard and the duplicate second guard. A single guard is now placed *after* `obj->wl_surface = nullptr` and *before* the observer loop — matching every other masked handler in the file:

```cpp
// Before:
if (obj->event_mask_.enabled && obj->event_mask_.all) { return; }  // wrong position
obj->wl_surface = nullptr;
if (obj->event_mask_.enabled && obj->event_mask_.all) { return; }  // dead code
for (const auto observer : obj->observers_) { … }

// After:
obj->wl_surface = nullptr;
if (obj->event_mask_.enabled && obj->event_mask_.all) { return; }  // single, correct guard
for (const auto observer : obj->observers_) { … }
```

`wl_surface` is now always cleared on every leave event regardless of the event mask, and observer notification is correctly suppressed only when masking is active.

---

### HIGH-2 — Race condition on `volatile bool gRunning` / `running` in examples ✅ FIXED  
**Files:** `examples/simple-shm.cc`, `examples/simple-egl.cc`, `examples/agl-simple-shm.cc`

**Problem:** `volatile` does not provide memory ordering guarantees on multi-core systems. On architectures with weak memory models, the write from the signal handler and the read in the main loop can be reordered or cached. The C++11 standard requires `std::atomic<bool>` (or `sig_atomic_t` for signal safety) for variables shared between signal handlers and regular code.

**Fix applied (2026-02-25):**  
In all three files:
- Added `#include <atomic>`.
- `static volatile bool gRunning` / `static volatile bool running` → `static std::atomic<bool> gRunning{true}` / `static std::atomic<bool> running{true}`.
- `handle_signal`: `gRunning = false` → `gRunning.store(false, std::memory_order_relaxed)`. A relaxed store is sufficient for a signal handler writing a stop flag — the signal delivery itself is the synchronisation point with the OS.
- Run-loop condition: `while (gRunning && …)` → `while (gRunning.load(std::memory_order_acquire) && …)`. The acquire load ensures all side-effects of the signal handler are visible before the loop body checks `is_valid()` and dispatches.

---

### HIGH-3 — `scene_initialized` volatile race in `simple-egl` ✅ FIXED  
**File:** `examples/simple-egl.cc`

**Problem:** Same root cause as HIGH-2. `volatile bool scene_initialized` provided no atomicity or ordering guarantee. File-scope placement meant any future multi-threaded use would be a data race.

**Fix applied (2026-02-25):**  
Fixed alongside HIGH-2 in the same file:
- `volatile bool scene_initialized` → `static std::atomic<bool> scene_initialized{false}`.
- Read in `draw_frame`: `if (!scene_initialized)` → `if (!scene_initialized.load(std::memory_order_acquire))`.
- Write in `draw_frame`: `scene_initialized = true` → `scene_initialized.store(true, std::memory_order_release)`. The release store pairs with the acquire load, providing a proper happens-before edge ensuring `initialize_scene`'s writes are visible before the flag is observed as `true`.

---

### HIGH-4 — EGL surface used before creation (`eglMakeCurrent` ordering bug) ✅ FIXED  
**File:** `src/window/egl.cc` — `Egl::Egl()` constructor

**Problem:** `eglMakeCurrent` was called with `egl_surface_` before `egl_surface_` was assigned by `eglCreateWindowSurface`. At that point `egl_surface_` was still `EGL_NO_SURFACE` (default-initialized to `nullptr`). The call was therefore a no-op at best, or produced a silent EGL error on strict implementations. The intended order is: create wl_egl_window → create EGL surface → make current.

```cpp
// Before (wrong order):
wl_egl_window_ = wl_egl_window_create(...);
eglMakeCurrent(dpy_, egl_surface_, egl_surface_, context_);  // egl_surface_ == EGL_NO_SURFACE ← bug
egl_surface_ = eglCreateWindowSurface(...);
eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
```

**Fix applied (2026-02-25):**  
The premature `eglMakeCurrent(dpy_, egl_surface_, egl_surface_, context_)` call was removed entirely. No replacement is needed: the constructor is not required to leave the context current — that is the job of `Egl::make_current()` which callers invoke explicitly before rendering. The final `eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)` that clears the context after construction is preserved.

```cpp
// After (correct):
wl_egl_window_ = wl_egl_window_create(...);
egl_surface_ = eglCreateWindowSurface(...);                  // surface created first
eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);  // clear, as before
```

---

### HIGH-5 — `xkb_state_update_mask` called with potentially NULL `xkb_state_` ✅ FIXED  
**File:** `src/seat/keyboard.cc` — `handle_modifiers()`

**Problem:** No null-check for `obj->xkb_state_` existed before the `xkb_state_update_mask` call inside the `WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1` branch. If `handle_keymap` failed (CRIT-4, now fixed) or a modifier event arrived before any keymap was sent by the compositor, `xkb_state_` would be `nullptr` and the call would be undefined behaviour. `handle_key` correctly guards with `if (!obj->xkb_state_) return;` — `handle_modifiers` was inconsistently unguarded.

**Fix applied (2026-02-25):**  
Added `if (!obj->xkb_state_) return;` immediately before `xkb_state_update_mask`, inside the `XKB_V1` format branch, consistent with the guard in `handle_key`:

```cpp
if (obj->format_ == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    if (!obj->xkb_state_)   // ← added
        return;
    xkb_state_update_mask(obj->xkb_state_, mods_depressed, mods_latched,
                          mods_locked, 0, 0, group);
}
```

---

### HIGH-6 — `popen` called with empty string after silent sanitization ✅ FIXED  
**File:** `src/command.cc` — `Execute()` / `sanitize_cmd()`

**Problem:** `sanitize_cmd` silently discards every character not in the safe set (alphanumeric, space, `_`, `-`, `/`, `.`). If all characters in the original command were unsafe the function returned an empty string, and `popen("")` was called unconditionally. Passing an empty string to `popen` is implementation-defined — on Linux glibc it spawns `/bin/sh -c ""` (a shell with no command), silently returning an open pipe with no output rather than an error. The existing `cmd.empty()` guard at the top of `Execute` only checked the original unsanitized input.

**Fix applied (2026-02-25):**  
Added a post-sanitization emptiness check in `Execute()` immediately after `sanitize_cmd`:

```cpp
const std::string safe_cmd = sanitize_cmd(cmd);
if (safe_cmd.empty()) {
    spdlog::error("[Command] Execute: command '{}' reduced to empty string "
                  "after sanitization — refusing to execute", cmd);
    return false;
}
FILE* fp = popen(safe_cmd.c_str(), "r");
```

Also fixed **LOW-1** in the same files: `is_safe_char` return type changed from `int` to `bool` in both `command.h` (declaration) and `command.cc` (definition). The function already returned a boolean expression; the `int` return type was a legacy holdover from C's `isalnum` convention.

---

### HIGH-7 — Pointer cursor surface committed with zero damage when cursor is disabled ✅ FIXED  
**File:** `src/seat/pointer.cc` — `Pointer::set_cursor()`

**Problem:** When `disable_cursor_` was set, the code called `wl_pointer_set_cursor` with `wl_surface_cursor_` (a live surface) and then called `wl_surface_damage(…, 0, 0, 0, 0)` + `wl_surface_commit`. This is protocol-incorrect: committing a surface with zero damage and no attached buffer produces undefined compositor behaviour and may generate compositor-side warnings. The correct Wayland protocol mechanism to hide the cursor is to pass `NULL` as the surface argument to `wl_pointer_set_cursor`, which the protocol explicitly documents for this purpose.

**Fix applied (2026-02-25):**  
The three-line `disable_cursor_` branch was replaced with a single protocol-correct call:

```cpp
// Before:
if (disable_cursor_) {
    wl_pointer_set_cursor(wl_pointer_, serial, wl_surface_cursor_, 0, 0);
    wl_surface_damage(wl_surface_cursor_, 0, 0, 0, 0);  // zero damage
    wl_surface_commit(wl_surface_cursor_);              // commits nothing
    return;
}

// After:
if (disable_cursor_) {
    wl_pointer_set_cursor(wl_pointer_, serial, nullptr, 0, 0);  // hides cursor
    return;
}
```

The hotspot arguments (`0, 0`) are ignored by the compositor when the surface is `NULL` but are kept for protocol conformance. The cursor surface (`wl_surface_cursor_`) is left unmodified, which is correct — it is only used when a visible cursor is needed.

---

### HIGH-8 — `activate_app` uses `it` after `erase` (iterator invalidation) ✅ FIXED  
**File:** `src/window_manager/agl_shell.cc` — `activate_app()`

**Problem:** Two instances of undefined behaviour in the original code:

1. `find_output_by_name(it->second)` and `find_output_by_name("remoting-" + it->second)` were called **before** `pending_app_list_.erase(it)`. This ordering was safe for the lookups themselves, but `erase` was placed **after** both lookups — then `it->second` was read a third time in the `DLOG_DEBUG` call **outside** the `if` block, after `erase` had already invalidated the iterator. That read is undefined behaviour.

2. The `DLOG_DEBUG("[AGL] Activating app_id {} on output {}", app_id, it->second)` line was placed unconditionally after the `if` block, meaning it also fired when `it == pending_app_list_.end()` (no pending entry found), dereferencing `end()` — undefined behaviour regardless of the erase.

**Fix applied (2026-02-25):**

- The output name is saved to a local `const std::string output_name = it->second` **before** `erase` is called.  
- `erase` is now called **first** (immediately after saving the local), before any use of the output name — eliminating the iterator invalidation window.  
- The `find_output_by_name` calls are updated to use `output_name` instead of `it->second`.  
- The `DLOG_DEBUG` that logged the output name is moved **inside** the `if` block (after the successful output lookup) using `output_name`, so it only fires when a pending entry was actually found and an output was resolved.

```cpp
// Before (UB: it used after erase, and unconditionally when it == end()):
wl_output = find_output_by_name(it->second);
// ...
pending_app_list_.erase(it);          // iterator invalidated here
// ...
DLOG_DEBUG("... on output {}", app_id, it->second);  // UB: it is invalid

// After (safe):
const std::string output_name = it->second;  // save before erase
pending_app_list_.erase(it);                 // it now safely discarded
wl_output = find_output_by_name(output_name);
// ...
DLOG_DEBUG("... on output {}", app_id, output_name);  // inside if block, uses local
```

---

## Medium Severity Issues

### MED-1 — `Output::handle_done` operates on a local copy, not `obj->output_` ✅ FIXED  
**File:** `src/window_manager/output.cc` — `handle_done()`

**Problem:** `auto output = obj->output_` created a value copy of the struct. Setting `output.done = true` wrote to that temporary copy, leaving `obj->output_.done` permanently `false`. Every call to `Output::print()` and any code checking the `done` flag would always observe `false`.

**Fix applied (2026-02-25):**  
Changed `auto output` to `auto& output` so the assignment binds a reference to the actual member:

```cpp
// Before — writes to a discarded local copy:
auto output = obj->output_;
output.done = true;

// After — writes to the member directly:
auto& output = obj->output_;
output.done = true;
```

---

### MED-2 — `handle_scale` logs "enter" twice (copy-paste log error) ✅ FIXED  
**File:** `src/window_manager/output.cc` — `handle_scale()`

**Problem:** The exit trace log used `++Output::handle_scale()` instead of `--Output::handle_scale()`, making trace log parsing unreliable (two entry markers, no exit marker).

**Fix applied (2026-02-25):**  
Changed the second `LOG_TRACE("++Output::handle_scale()")` to `LOG_TRACE("--Output::handle_scale()")`.

Both MED-1 and MED-2 were fixed in the same edit to `output.cc`.

---

### MED-3 — `Feedback::sequence_` is a non-atomic static counter accessed unsafely ✅ FIXED  
**File:** `src/window/feedback.cc` / `include/waypp/window/feedback.h` — `Feedback::Feedback()`

**Problem:** `sequence_` was a plain `static unsigned` incremented with `++sequence_` in the constructor. If `Feedback` objects were ever constructed from multiple threads (e.g. from different window frame callbacks executing concurrently), this increment is a data race — undefined behaviour under the C++ memory model regardless of platform.

**Fix applied (2026-02-25):**

- **`feedback.h`**: added `#include <atomic>`; changed `static unsigned sequence_` → `static std::atomic<unsigned> sequence_`.
- **`feedback.cc`**: changed the definition `unsigned Feedback::sequence_ = 0` → `std::atomic<unsigned> Feedback::sequence_{0}`.
- **Constructor**: replaced `frame_no_ = ++sequence_` with `frame_no_ = sequence_.fetch_add(1u, std::memory_order_relaxed) + 1u`.

  `fetch_add` with `memory_order_relaxed` is sufficient — the counter is used only for logging/identification; there is no ordering dependency between the increment and any other shared state. The `+ 1u` preserves the original pre-increment semantics: `frame_no_` receives the value *after* the increment (i.e. the first frame is numbered 1, not 0).

---

### MED-4 — `Buffer::create_shm_buffer` uses hardcoded `pitch = width * 4` bytes-per-pixel ✅ FIXED  
**File:** `src/window/buffer.cc` / `include/waypp/window/buffer.h`

**Problem:** Stride was always computed as `width * 4` regardless of `format`. For any `wl_shm_format` that is not 32 bpp (e.g. `WL_SHM_FORMAT_RGB565` at 2 bytes/pixel, `WL_SHM_FORMAT_RGB888` at 3 bytes/pixel, `WL_SHM_FORMAT_C8` at 1 byte/pixel), this produced an incorrect pool size and stride, causing out-of-bounds buffer mapping or a corrupt image.

**Fix applied (2026-02-25):**
- Added a `switch` on `static_cast<wl_shm_format>(format)` covering the full range of packed formats defined in `wayland-client-protocol.h`, mapping each to its correct bytes-per-pixel (1, 2, 3, or 4). Any format not in the switch logs an error at ERROR level and returns `-1` — preventing creation of a buffer with an unknown stride rather than silently producing a corrupt one.
- `pitch` is now computed as `width * bpp` using the looked-up value; `size_` derives from that.
- Added `[[nodiscard]] int get_size() const { return size_; }` to `buffer.h` so callers can retrieve the exact allocated byte count without recomputing stride themselves.

Also fixes **MED-6** (`src/window/window.cc` · `next_buffer()`): the `memset` that initialised padding used `width * height * 4` — the same hardcoded assumption. Replaced with `buffer->get_size()`, which is the authoritative byte count set by `create_shm_buffer`.

```cpp
// Before (MED-4 — buffer.cc):
const auto pitch = width * 4;   // wrong for non-32bpp formats

// After:
int bpp;
switch (static_cast<wl_shm_format>(format)) { … }  // format-aware lookup
const int pitch = width * bpp;

// Before (MED-6 — window.cc):
memset(shm_data, 0xff, width * height * 4);  // hardcoded bpp

// After:
memset(buffer->get_shm_data(), 0xff, static_cast<size_t>(buffer->get_size()));
```

---

### MED-5 — `draw_frame` in examples calls `exit()` on buffer acquisition failure ✅ FIXED  
**Files:** `examples/simple-shm.cc`, `examples/agl-simple-shm.cc` — `draw_frame()`

**Problem:** `draw_frame` is registered as a `wl_surface_frame` callback. Calling `exit()` from inside this callback bypasses all RAII destructors — Wayland object destruction (`wl_display_flush`, `wl_keyboard_release`, `wl_pointer_destroy`, etc.) never runs, leaving the compositor with dangling client state. The `wl_display_disconnect` in `App::~App()` is also skipped.

**Fix applied (2026-02-25):**  
Replaced `exit(EXIT_FAILURE)` with a three-step graceful shutdown, identical in both files:

```cpp
// Before:
if (!buffer) {
    spdlog::error("Failed to acquire a buffer");
    exit(EXIT_FAILURE);         // skips all RAII cleanup
}

// After:
if (!buffer) {
    spdlog::error("[draw_frame] Failed to acquire a buffer — stopping render loop");
    window->stop_frame_callbacks();                      // halts the frame-callback chain
    window->close();                                     // sets valid_ = false
    gRunning.store(false, std::memory_order_relaxed);    // signals the dispatch loop
    return;
}
```

- `stop_frame_callbacks()` — destroys the pending `wl_callback_` and prevents the next `wl_surface_frame` from being requested, halting the render chain at the Wayland level.
- `close()` — sets `valid_ = false`, causing `toplevel_->is_valid()` in the `while` run-loop condition to return `false`, exiting dispatch normally.
- `gRunning.store(false)` — belt-and-suspenders: ensures the `gRunning` condition in the run loop also evaluates to `false` on the next iteration, even if `is_valid()` is somehow still true.

All three together cause the `main()` run loop to exit on the next iteration, at which point `App::~App()` runs normally — destroying the `XdgTopLevel`, flushing and disconnecting the Wayland display, and releasing all resources in the correct order.

---

### MED-6 — `Window::next_buffer` memset uses hardcoded `* 4` ✅ FIXED  
**File:** `src/window/window.cc` — `next_buffer()`

Hardcoded `width * height * 4` replaced with `buffer->get_size()` (added to `Buffer` as part of MED-4). See MED-4 for full details.

---

### MED-7 — `set_event_mask` logic for touch event is inverted ✅ FIXED  
**File:** `src/seat/seat.cc` — `set_event_mask()`

**Problem:** The `touch` branch had `.all` and `.enabled` assigned in the wrong order relative to the `pointer` and `keyboard` branches:

```cpp
// pointer/keyboard pattern (correct):
event_mask_.keyboard.enabled = true;     // set .enabled whenever prefix matches
if (event == "keyboard") {
    event_mask_.keyboard.all = true;     // set .all only on exact match
}

// touch branch (wrong — inverted):
event_mask_.touch.all = true;            // .all always set on any "touch*" prefix
if (event == "touch") {
    event_mask_.touch.enabled = true;    // .enabled only set on exact match
}
```

The consequence: any `touch` sub-event string (e.g. a hypothetical future `"touch-cancel"`) would set `.all = true` — masking all touch events — while `.enabled` would remain `false`, leaving the touch device in a contradictory state. For the currently supported `"touch"` exact match both fields ended up set, so the bug was latent rather than immediately observable.

**Fix applied (2026-02-25):**  
Swapped the assignments to mirror the `keyboard` (and `pointer`) pattern exactly:

```cpp
} else if (event.rfind("touch", 0) == 0) {
    event_mask_.touch.enabled = true;    // set .enabled whenever prefix matches
    if (event == "touch") {
        event_mask_.touch.all = true;    // set .all only on exact "touch" match
    }
    if (touch_) {
        touch_->set_event_mask(event_mask_.touch);
    }
}
```

---

### MED-8 — `Pointer::get_available_cursors()` shells out to `ls` with unsanitized theme name ✅ FIXED  
**File:** `src/seat/pointer.cc` — `get_available_cursors()`

**Problem:** The theme name from `get_cursor_theme()` (sourced from `gsettings`) was interpolated directly into a shell command string:

```cpp
ss << "ls -1 /usr/share/icons/" << theme << "/cursors";
Command::Execute(ss.str(), res);
```

`Command::sanitize_cmd` allows `/`, `-`, `.`, space, alphanumerics, and `_`. A cursor-theme value containing e.g. `Adwaita; rm -rf ~` would survive partial sanitization (space and alphanumerics pass), and the semicolon alone is sufficient for command injection on many shells. Path traversal via `../../` also passes (`/` is allowed). Even setting the theme to an empty string would cause `Command::Execute` to be called on `"ls -1 /usr/share/icons//cursors"`.

**Fix applied (2026-02-25):**  
Two-layer defence:

1. **Strict theme name validation**: before any path construction, the theme name is checked character-by-character against `[a-zA-Z0-9_-]` only. Any character outside this set (including `/`, `.`, space, and all shell metacharacters) causes an immediate `LOG_ERROR` + `return {}`. This is stricter than `is_safe_char` and completely eliminates path traversal and injection.

2. **`opendir`/`readdir` instead of `ls`**: the shell-out is replaced with a direct POSIX directory traversal:
   ```cpp
   const std::string cursors_dir = "/usr/share/icons/" + theme + "/cursors";
   DIR* dir = opendir(cursors_dir.c_str());
   // ...
   while ((entry = readdir(dir)) != nullptr) { cursor_list.emplace_back(entry->d_name); }
   closedir(dir);
   ```
   No shell is spawned. `"."` and `".."` entries are skipped explicitly. `opendir` failure logs a `WARN` with `std::strerror(errno)` and returns an empty list.

Includes added: `<cerrno>`, `<cstring>`, `<dirent.h>`. Includes removed: `<sstream>` (no longer used).

---

### MED-9 — `Pointer` constructor sets `event_mask_` fields twice ✅ FIXED  
**File:** `src/seat/pointer.cc` — `Pointer::Pointer()`

**Problem:** All five `event_mask_` fields (`enabled`, `all`, `axis`, `buttons`, `motion`) were correctly initialised in the member-initializer list. The constructor body then redundantly re-assigned four of them (`enabled`, `axis`, `buttons`, `motion`) — `all` was notably absent from the body, revealing this as a stale copy-paste from an earlier refactoring. The body assignments were authoritative for nothing; they wrote the same values already present. While harmless today, this pattern creates confusion about which assignment is canonical and is a latent risk if the MIL and body assignments diverge during future edits.

**Fix applied (2026-02-25):**  
Removed the four redundant body assignments. The member-initializer list remains as the sole, authoritative initialisation of `event_mask_`:

```cpp
// Before — MIL + redundant body:
event_mask_({.enabled = event_mask.enabled,
             .all     = event_mask.all,
             .axis    = event_mask.axis,
             .buttons = event_mask.buttons,
             .motion  = event_mask.motion}) {
  // ...
  event_mask_.enabled = event_mask.enabled;  // redundant
  event_mask_.axis    = event_mask.axis;     // redundant
  event_mask_.buttons = event_mask.buttons;  // redundant
  event_mask_.motion  = event_mask.motion;   // redundant
}

// After — MIL only:
event_mask_({.enabled = event_mask.enabled,
             .all     = event_mask.all,
             .axis    = event_mask.axis,
             .buttons = event_mask.buttons,
             .motion  = event_mask.motion}) {
  // ...
}
```

---

## Low Severity / Code Quality Issues

### LOW-1 — `is_safe_char` returns `int` but should return `bool` ✅ FIXED  
**File:** `src/command.cc` / `src/command.h` — `Command::is_safe_char()`

`is_safe_char` returned `int` (a legacy holdover from C's `std::isalnum` convention) but was used exclusively in a boolean context. Fixed alongside HIGH-6: both the declaration in `command.h` and the definition in `command.cc` now return `bool`.

---

### LOW-2 — `Seat::event_mask_print()` constructs `out` as an empty `const std::string` then uses it as `stringstream` source ✅ FIXED  
**File:** `src/seat/seat.cc` — `event_mask_print()`

**Problem:** `out` was a default-constructed (empty) `const std::string` passed to the `std::stringstream` constructor purely as its initial content. Since it was empty it contributed nothing — `ss` was then built entirely via `operator<<` insertions. The declaration of `out` was dead code that added noise and implied `ss` was seeded from an existing string when it was not.

**Fix applied (2026-02-25):**  
Removed `const std::string out` and replaced `std::stringstream ss(out)` with a default-constructed `std::stringstream ss`. Also added explicit `#include <sstream>` and `#include <string>` to `seat.cc` so the file does not rely on transitive inclusion for types it uses directly.

```cpp
// Before — dead variable:
const std::string out;          // empty, contributes nothing
std::stringstream ss(out);      // misleadingly implies ss is seeded from out
ss << "Seat Event Mask";

// After — direct construction:
std::stringstream ss;
ss << "Seat Event Mask";
```

---

### LOW-3 — `std::list` used for observer lists instead of `std::vector` ✅ FIXED  
**Files:** `include/waypp/seat/keyboard.h`, `include/waypp/seat/pointer.h`, `include/waypp/seat/touch.h`, `include/waypp/seat/seat.h`, `include/waypp/window_manager/window_manager.h`, `include/waypp/window_manager/weston-capture.h`

**Problem:** All six observer lists were `std::list<T*>`. Observer lists in this codebase are registered once, iterated on every input event, and rarely removed from. `std::list` incurs a heap allocation per node, poor cache locality, and pointer indirection on every iteration — exactly the wrong tradeoffs for hot-path event dispatch.

**Fix applied (2026-02-25):**
- `#include <list>` → `#include <algorithm>` + `#include <vector>` in each header.
- `std::list<T*> observers_{}` → `std::vector<T*> observers_{}` in all six classes.
- `observers_.remove(observer)` (a `std::list` member function) → the C++17-compatible erase-remove idiom:
  ```cpp
  observers_.erase(
      std::remove(observers_.begin(), observers_.end(), observer),
      observers_.end());
  ```
- `push_back` and range-for iteration are unchanged — both work identically on `std::vector`.
- The `std::list` members in `agl_shell.h` (`apps_stack_`, `pending_app_list_`) were intentionally left as `std::list`: `apps_stack_` uses `list::remove` by value on an arbitrarily-positioned element (correct use), and `pending_app_list_` uses iterator-stable `erase` after `find_if`.

### LOW-4 — `wl_display_` member shadows constructor parameter name in `WindowManager` ✅ FIXED  
**File:** `src/window_manager/window_manager.cc` / `include/waypp/window_manager/window_manager.h` / `include/waypp/window_manager/registrar.h`

**Problem:** `WindowManager` stored its own private `wl_display_` member, duplicating the `wl_display_` already stored by its `Registrar` base class. The `WindowManager` constructor initialised both from the same `display` parameter. This double-storage created coupling risk: if either pointer were ever updated independently the two copies would diverge silently. `Registrar` had no `get_display()` accessor, forcing `WindowManager` to maintain the redundant copy and its own `get_display()` override that returned the local duplicate rather than the base-class value.

**Fix applied (2026-02-25):**

1. **`registrar.h`** — added `[[nodiscard]] wl_display* get_display() const { return wl_display_; }` to `Registrar`'s public interface. `wl_display_` remains private to `Registrar` — subclasses access it exclusively through this accessor.

2. **`window_manager.h`** — removed the redundant `wl_display_` private member and the shadowing `get_display()` override. Updated `dispatch_pending()` (which was inline in the header) to call `get_display()` instead of the now-removed `wl_display_`.

3. **`window_manager.cc`** — removed `wl_display_(display)` from the constructor member-initializer list. Replaced all 14 uses of `wl_display_` in `dispatch()`, `poll_events()`, and `display_dispatch()` with `get_display()`. There is now a single source of truth for the display pointer: `Registrar::wl_display_`.

---

### LOW-5 — `touch_` member name conflicts with parameter name in `Touch::~Touch` ✅ FIXED  
**Files:** `include/waypp/seat/touch.h`, `src/seat/touch.cc`

**Problem:** Two naming inconsistencies existed in the `Touch` class:

1. The private Wayland handle member was named `touch_` (`wl_touch*`) while the analogous member in `Pointer` is named `wl_pointer_` (`wl_pointer*`). The inconsistency made the seat device classes harder to navigate.

2. The constructor parameter `wl_touch* wl_touch` shadowed the `wl_touch` type name, and the `handle_*` static callback parameters named `touch` shadowed the `Touch` class name. Both caused confusing name-lookup behaviour and compiler warnings.

3. A redundant `event_mask_.enabled = event_mask.enabled` body assignment was present in the constructor, duplicating the MIL initialiser (same pattern as MED-9).

**Fix applied (2026-02-25):**

- **`touch.h`**: `struct wl_touch* touch_` → `struct wl_touch* wl_touch_`. Constructor declaration parameter renamed from `wl_touch* wl_touch` → `wl_touch* touch_device`.
- **`touch.cc`** constructor: parameter renamed to `touch_device`; MIL updated to `wl_touch_(touch_device)`; `wl_touch_add_listener(touch_device, …)` updated; redundant body assignment removed.
- **`touch.cc`** destructor: `wl_touch_release(touch_)` → `wl_touch_release(wl_touch_)`.
- **`touch.cc`** all five `handle_*` callbacks (`handle_down`, `handle_up`, `handle_motion`, `handle_cancel`, `handle_frame`): parameter `wl_touch* touch` → `wl_touch* wl_touch_obj`; all `obj->touch_` comparisons → `obj->wl_touch_`; observer call arguments updated. Doc-comment `@param wl_touch` tags updated to match.

---

### LOW-6 — `simple-egl.cc` uses file-scope globals for `toplevel_`, `seat_`, `config`, `gl` ✅ FIXED  
**File:** `examples/simple-egl.cc`

**Problem:** Nine pieces of mutable state were scattered as file-scope globals: `Configuration config`, `struct { … } gl`, `std::shared_ptr<XdgTopLevel> toplevel_`, `Seat* seat_`, `uint32_t frames`, `uint32_t initial_frame_time`, `uint32_t benchmark_time`. This prevented unit-testing, re-use as a library component, and made the control flow opaque. `simple-shm.cc` wraps equivalent state in an `App` class; `simple-egl.cc` was inconsistent.

**Fix applied (2026-02-25):**  
All nine mutable globals consolidated into a single `EglApp` struct with nested `Configuration` and `GlState` sub-structs. `running` and `scene_initialized` remain as file-scope `std::atomic<bool>` (the signal handler must reach `running` without a pointer).

- `draw_triangle(Window*, EGLint)` → `draw_triangle(Window*, EGLint, const EglApp&)` — reads `app.gl.*` and `app.config.*`.
- `draw_frame(void*, uint32_t)` — casts `userdata` to `EglApp*` (via `*static_cast<EglApp*>(userdata)`). Retrieves the `Window*` as `static_cast<Window*>(app.toplevel_.get())`. Passes `app` to helpers.
- `class Observer` — given `explicit Observer(EglApp& app)` constructor; `notify_pointer_motion` and `notify_pointer_button` access `app_.toplevel_` and `app_.seat_` instead of globals.
- `main()` — constructs `EglApp app` on the stack, fills `app.config`, passes `&app` as `user_data` to `start_frame_callbacks()`.

### LOW-7 — `glUniformMatrix4fv` passes `(GLfloat*)rotation.d` C-style cast ✅ FIXED  
**File:** `examples/simple-egl.cc` — `draw_frame()`

**Problem:** `(GLfloat*)rotation.d` is a C-style cast that bypasses compiler type checks. `rotation.d` is already `float[16]` which implicitly converts to `const GLfloat*`; no cast is needed at all.

**Fix applied (2026-02-25):** The C-style cast was removed. `rotation.d` is passed directly — the array-to-pointer decay provides `const GLfloat*` implicitly and the compiler verifies the type match.

---

### LOW-8 — Feedback list is never pruned in `Window` ✅ FIXED  
**Files:** `include/waypp/window/feedback.h`, `src/window/feedback.cc`, `src/window/window.cc`

**Problem:** Every frame in presentation mode, `handle_frame_callback` pushed a new `Feedback` object onto `presentation_.feedback_list`. The `handle_presented` and `handle_discarded` Wayland callbacks in `feedback.cc` notified an optional observer but never removed the entry from the list. For long-running sessions the list grew without bound, leaking `wp_presentation_feedback` Wayland objects and heap memory every frame.

**Fix applied (2026-02-25):**

1. **`feedback.h`** — added `#include <functional>` and a public `set_on_done(std::function<void(Feedback*)>)` setter plus a private `on_done_` member. The hook is called by the `Feedback` itself on terminal events (presented or discarded), passing `this` as the argument, so the owner can locate and erase the entry without `Feedback` needing to know about `std::list` or `Window`.

2. **`feedback.cc`** — added `if (f->on_done_) { f->on_done_(f); }` at the end of both `handle_presented` and `handle_discarded`, after the observer notification.

3. **`window.cc`** — added `#include <algorithm>`. After constructing each `Feedback` and before pushing it into `feedback_list`, stash the raw pointer and call `set_on_done` with a lambda that captures `obj` and uses the erase-remove_if idiom to remove the completed entry:
   ```cpp
   Feedback* raw = feedback.get();
   raw->set_on_done([obj](Feedback* done) {
     auto& list = obj->presentation_.feedback_list;
     list.erase(std::remove_if(list.begin(), list.end(),
                               [done](const std::unique_ptr<Feedback>& p) {
                                 return p.get() == done;
                               }),
                list.end());
   });
   obj->presentation_.feedback_list.push_back(std::move(feedback));
   ```
   The lambda runs from within the Wayland callback (on the `Feedback` still alive on the stack). `remove_if` moves the matching `unique_ptr` to the end; `erase` destroys it, which calls `Feedback::~Feedback()` → `wp_presentation_feedback_destroy`. This is safe because `handle_presented`/`handle_discarded` do not access `f` after invoking `on_done_`.

---

### LOW-9 — `agl-simple-shm.cc` uses internal non-public headers ✅ FIXED  
**File:** `examples/agl-simple-shm.cc`

**Problem:** Two `#include` directives used bare internal paths:
```cpp
#include "window/xdg_toplevel.h"
#include "window_manager/agl_shell.h"
```
These bypass the public `waypp/` prefix used by every other example and by the installed include layout. They only resolve because the build system adds the `include/waypp/` subtree to the search path as a private directory. When the library is installed or consumed from a package, these paths break.

**Fix applied (2026-02-25):**  
Both paths updated to use the canonical public prefix:
```cpp
#include "waypp/window/xdg_toplevel.h"
#include "waypp/window_manager/agl_shell.h"
```
This matches the pattern used in `simple-shm.cc`, `simple-egl.cc`, `simple-ext-protocol.cc`, and all other examples.

---

## Per-File Findings

| File | Issues |
|------|--------|
| `src/seat/keyboard.cc` | CRIT-1, CRIT-2, CRIT-4, HIGH-1, HIGH-5 |
| `src/seat/pointer.cc` | HIGH-7, HIGH-8 (via `agl_shell`), MED-8, MED-9, LOW-3 |
| `src/seat/touch.cc` | LOW-5 |
| `src/seat/seat.cc` | MED-7, LOW-2 |
| `src/window/egl.cc` | HIGH-4 |
| `src/window/buffer.cc` | MED-4 |
| `src/window/feedback.cc` | MED-3, LOW-8 |
| `src/window/window.cc` | MED-6, LOW-8 |
| `src/window/xdg_toplevel.cc` | CRIT-3 |
| `src/window_manager/agl_shell.cc` | CRIT-3, HIGH-8 |
| `src/window_manager/output.cc` | MED-1, MED-2 |
| `src/window_manager/registrar.cc` | (clean, good error handling) |
| `src/window_manager/xdg_window_manager.cc` | CRIT-3 |
| `src/window_manager/window_manager.cc` | LOW-4 |
| `src/command.cc` | HIGH-6, LOW-1 |
| `examples/simple-shm.cc` | CRIT-3, HIGH-2, MED-5 |
| `examples/simple-egl.cc` | CRIT-3, HIGH-2, HIGH-3, LOW-6, LOW-7 |
| `examples/agl-simple-shm.cc` | CRIT-3, HIGH-2, MED-5, LOW-9 |

---

## Positive Patterns

The following patterns are well-implemented and should be maintained:

- **RAII throughout**: Wayland objects are consistently wrapped in `std::unique_ptr` with appropriate destructors. All `wl_*_destroy` / `wl_*_release` calls are made in destructors.
- **Object identity validation in callbacks**: Every Wayland C callback begins with `if (obj->wl_xxx_ != wl_xxx) return;`, preventing stale/misrouted events.
- **`eglGetProcAddress` for extensions**: EGL extension function pointers are resolved at runtime and checked before use, avoiding hard-link failures on platforms that lack them.
- **MAP_PRIVATE keymap mmap (version-gated)**: `keyboard.cc` correctly uses `MAP_PRIVATE` for protocol version ≥ 7 per the Wayland specification.
- **`std::min` version clamping in registry binding**: All `wl_registry_bind` calls correctly clamp to `std::min(kXxxMinVersion, version)`, preventing binding a higher version than the compositor advertises.
- **`[[nodiscard]]` on getters**: Public API headers consistently apply `[[nodiscard]]` to query methods, helping catch silently-ignored return values.
- **No raw `new`/`delete`**: Heap allocations exclusively use `std::make_unique`/`std::make_shared`, eliminating double-free and leak risk from that source.
- **Copy/assignment disabled on resource-owning classes**: `Keyboard`, `Pointer`, `Touch`, `Window`, `Buffer`, and `Egl` all declare `= delete` for copy constructor and copy assignment operator.















