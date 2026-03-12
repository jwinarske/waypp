# wayland-cxx-scanner Integration

> **Status:** In progress — C++23 protocol header generation is implemented.
> Source migration (switching waypp from C API to C++ proxy API) is the next step.

## Architecture

`wayland-cxx-scanner` (<https://github.com/jwinarske/wayland-cxx-scanner>) is
a **host build tool** — it is installed separately in CI before the waypp build
begins, then found via `find_program()` exactly like `wayland-scanner`.

Only the **generated C++23 protocol headers** (`.hpp` files) produced by the
tool are consumed by the waypp build.  No framework library dependency
(`wayland-cxx` pkg-config, `wl/proxy_impl.hpp`, etc.) is required in
`wayland_gen_dep`; when source migration is complete those headers will be
pulled in naturally as part of the migrated source includes.

### What is generated per protocol XML

| Tool | Output | Purpose |
|---|---|---|
| `wayland-cxx-scanner --mode=client-header` | `*-client-protocol.hpp` | C++23 CRTP proxy header **← only generated header** |
| `wayland-scanner private-code` | `*-client-protocol.c` | `wl_interface` ABI data (still required by the Wayland client runtime) |

`wayland-scanner client-header` (`.h` C client-headers) is **not generated**.

---

## CI setup

Both Meson and CMake CI workflows install `wayland-cxx-scanner` as a host tool
before the waypp configure step:

```yaml
- name: Install packages
  run: |
    sudo apt-get install -y libpugixml-dev   # scanner dependency
    pip install meson                         # for building the scanner

- name: Build and install wayland-cxx-scanner
  run: |
    git clone --depth=1 https://github.com/jwinarske/wayland-cxx-scanner \
      /tmp/wayland-cxx-scanner
    meson setup /tmp/wayland-cxx-scanner-build /tmp/wayland-cxx-scanner \
      --buildtype=release --prefix=/usr/local
    ninja -C /tmp/wayland-cxx-scanner-build
    sudo ninja -C /tmp/wayland-cxx-scanner-build install
```

After installation:
- Tool: `/usr/local/bin/wayland-cxx-scanner`
- Framework headers (for future use): `/usr/local/include/wl/`

---

## Build system integration

### Meson (`meson.build`)

```meson
wl_scanner         = find_program('wayland-scanner')
wl_cxx_scanner_exe = find_program('wayland-cxx-scanner')

# Per-protocol generation (in the foreach loop):
hpp = custom_target(stem + '-hpp',
  output:  stem + '.hpp',
  command: [wl_cxx_scanner_exe, '--mode=client-header', xml, '@OUTPUT@'],
)
c = custom_target(stem + '-c',
  output:  stem + '.c',
  command: [wl_scanner, 'private-code', xml, '@OUTPUT@'],
)
```

### CMake (`cmake/wayland.cmake`)

```cmake
find_program(WAYLAND_SCANNER_EXECUTABLE     NAMES wayland-scanner     REQUIRED)
find_program(WAYLAND_CXX_SCANNER_EXECUTABLE NAMES wayland-cxx-scanner REQUIRED)

macro(wayland_generate protocol_file output_file)
    add_custom_command(OUTPUT ${output_file}.hpp
        COMMAND ${WAYLAND_CXX_SCANNER_EXECUTABLE}
                --mode=client-header ${protocol_file} ${output_file}.hpp
        DEPENDS ${protocol_file})
    list(APPEND WAYLAND_PROTOCOL_SOURCES ${output_file}.hpp)

    add_custom_command(OUTPUT ${output_file}.c
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code
                < ${protocol_file} > ${output_file}.c
        DEPENDS ${protocol_file})
    list(APPEND WAYLAND_PROTOCOL_SOURCES ${output_file}.c)
endmacro()
```

---

## Source migration (next step)

The waypp library source currently uses the C API (raw `wl_*` function calls
and C struct callbacks) that was previously provided by the `wayland-scanner
client-header` `.h` files.  Now that those `.h` files are no longer generated,
the source must be migrated to the C++23 CRTP proxy API from the generated
`.hpp` files.

### Migration pattern

| Before (C API) | After (C++23 proxy API) |
|---|---|
| `struct agl_shell* agl_shell_` | proxy member of generated type |
| `agl_shell_add_listener(agl_shell_, &listener, this)` | virtual `OnEvent()` overrides |
| `agl_shell_set_ready(agl_shell_)` | `agl_shell_.SetReady()` |
| `agl_shell_destroy(agl_shell_)` | `agl_shell_.Destroy()` |
| `AGL_SHELL_APP_STATE_STARTED` (bare C enum) | `AglShellAppState::Started` (scoped enum class) |

### Files requiring migration

| File | Reason |
|---|---|
| `include/waypp/window_manager/agl_shell.h` | includes generated AGL shell header |
| `src/window_manager/agl_shell.cc` | uses AGL shell C API |
| `src/window_manager/registrar.cc` | uses `*_interface.name` + C bind calls for all protocols |
| `src/window_manager/xdg_window_manager.cc` | uses XDG shell C API |
| `src/window/xdg_toplevel.cc` | uses XDG toplevel C API |
| `src/window_manager/ivi_wm.cc` | uses IVI WM C API |
| `src/window_manager/ivi_window_manager.cc` | uses IVI shell C API |
| `src/window/ivi_surface.cc` | uses IVI surface C API |
| `examples/presentation-shm.cc` | direct include of presentation-time header |
| `examples/simple-ext-protocol.cc` | direct include of xdg-output header |

### Acceptance criteria for source migration

- [ ] All generated `.hpp` files are included in source (not `.h`)
- [ ] All C API function calls replaced with C++ proxy method calls
- [ ] All C struct listener registrations replaced with virtual overrides
- [ ] All bare C enum values replaced with scoped `enum class` equivalents
- [ ] Both Meson and CMake CI are green with examples building
