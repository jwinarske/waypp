# wayland-cxx-scanner Integration Plan

> **Status:** Pending — implement after the Meson CI is green.

## Background

`wayland-cxx-scanner` (<https://github.com/jwinarske/wayland-cxx-scanner>) is a
C++23 replacement for the standard `wayland-scanner` tool.  Where
`wayland-scanner` emits plain C headers and private-code `.c` files,
`wayland-cxx-scanner` emits idiomatic C++23 CRTP-based proxy classes.

The scanner provides two things:

| Artifact | Location | Purpose |
|---|---|---|
| **Scanner tool** (`wayland-cxx-scanner`) | built from `src/` | Reads a protocol XML, writes a C++23 header |
| **Framework headers** (`include/wl/`) | installed to `${includedir}/wl/` | Base classes / helpers consumed by every generated header |

### Scanner modes

| `--mode` flag | Output | Replaces |
|---|---|---|
| `client-header` | C++23 client-proxy header (`.hpp`) | `wayland-scanner client-header` |
| `server-header` | C++23 server-resource header (`.hpp`) | `wayland-scanner server-header` |
| `c-header` | C-style client header (`.h`) | `wayland-scanner client-header` (backward-compat) |

`wayland-scanner private-code` (`.c` files for `wl_interface` data) is **still
needed** — `wayland-cxx-scanner` does not replace this step.

---

## Phases

### Phase 1 — Add the scanner as a Meson subproject

1. Create `subprojects/wayland-cxx-scanner.wrap`:

   ```ini
   [wrap-git]
   url = https://github.com/jwinarske/wayland-cxx-scanner
   revision = main
   depth = 1

   [provide]
   wayland-cxx-scanner = wayland_cxx_scanner_exe
   ```

2. In `meson.build`, resolve the subproject and grab the executable:

   ```meson
   wl_cxx_scanner_sp  = subproject('wayland-cxx-scanner')
   wl_cxx_scanner_exe = wl_cxx_scanner_sp.get_variable('wayland_cxx_scanner_exe')
   wl_cxx_fw_dep      = wl_cxx_scanner_sp.get_variable('wayland_cxx_dep')
   ```

3. Export `wl_cxx_fw_dep` through `wayland_gen_dep` so all consumers
   automatically get the `wl/` framework headers.

### Phase 2 — Generate C++23 protocol headers alongside C headers

Extend `meson.build`'s `foreach` protocol loop to emit a `.hpp` as well as the
existing `.h` and `.c`:

```meson
foreach p : proto_list
  xml  = p[0]
  stem = p[1]
  flag = p[2]

  if fs.is_file(xml)
    # Existing C artifacts (keep for wl_interface ABI data)
    h = custom_target(stem + '-h', ...)
    c = custom_target(stem + '-c', ...)   # wayland-scanner private-code

    # New: C++23 proxy header
    hpp = custom_target(stem + '-hpp',
      output:  stem + '.hpp',
      command: [wl_cxx_scanner_exe, '--mode=client-header', xml, '@OUTPUT@'],
    )

    wayland_proto_h   += [h, hpp]
    wayland_proto_c   += [c]
    active_proto_flags += [flag]
  endif
endforeach
```

### Phase 3 — Expose framework headers to waypp consumers

Update `wayland_gen_dep` to pull in the framework:

```meson
wayland_gen_dep = declare_dependency(
  link_with:           wayland_gen_lib,
  include_directories: [include_directories('include'), build_root_inc],
  dependencies:        [wayland_client, wl_cxx_fw_dep],
)
```

This makes `#include <wl/proxy.hpp>` available everywhere that links
`wayland-gen` — both the waypp library and examples.

### Phase 4 — CMake support

`wayland-cxx-scanner` is a Meson project.  CMake can consume it via
`ExternalProject_Add`:

1. Add `cmake/WaylandCxxScanner.cmake`:

   ```cmake
   include(ExternalProject)
   ExternalProject_Add(wayland-cxx-scanner-build
     GIT_REPOSITORY https://github.com/jwinarske/wayland-cxx-scanner
     GIT_TAG        main
     CONFIGURE_COMMAND meson setup <BINARY_DIR> <SOURCE_DIR>
                       --prefix=<INSTALL_DIR>
                       --buildtype=${CMAKE_BUILD_TYPE_LOWER}
     BUILD_COMMAND     ninja -C <BINARY_DIR>
     INSTALL_COMMAND   ninja -C <BINARY_DIR> install
   )
   ExternalProject_Get_Property(wayland-cxx-scanner-build INSTALL_DIR)
   set(WAYLAND_CXX_SCANNER_EXE
       "${INSTALL_DIR}/bin/wayland-cxx-scanner" CACHE FILEPATH "" FORCE)
   set(WAYLAND_CXX_INCLUDE_DIR
       "${INSTALL_DIR}/include"               CACHE PATH "" FORCE)
   ```

2. Add a `wayland_generate_cxx()` macro in `cmake/wayland.cmake` that calls
   `WAYLAND_CXX_SCANNER_EXE --mode=client-header` and appends the `.hpp` to
   `WAYLAND_PROTOCOL_SOURCES` alongside the existing C artifacts.

3. Add `${WAYLAND_CXX_INCLUDE_DIR}` to `wayland-gen`'s PUBLIC include
   directories so all consumers see the `wl/` headers.

### Phase 5 — Migrate waypp source to C++23 API

Once the generated `.hpp` files are available and the framework headers are on
the include path, waypp source can be ported incrementally:

| File | Change |
|---|---|
| `include/waypp/window_manager/registrar.h` | Replace `wl_registry_listener` callback struct with `wl::Registry<Registrar>` |
| `src/window_manager/registrar.cc` | Use `wl::wl_ptr<wl_compositor>` etc. instead of raw pointers |
| `src/window_manager/agl_shell.cc` | Include `agl-shell-client-protocol.hpp`; use scoped enums |
| `src/seat/keyboard.cc` | Replace `wl_keyboard_listener` with generated `XdgShell::KeyboardListener` |
| `include/waypp/window_manager/xdg_window_manager.h` | Include `xdg-shell-client-protocol.hpp` |

**Enum migration:** `wayland-cxx-scanner` generates `enum class` values.
Code using bare enum values such as `AGL_SHELL_APP_STATE_STARTED` must be
updated to the scoped form `agl_shell_app_state::started` (name mapping follows
the scanner's `name_transform.cpp`).

**Backwards compatibility:** The C `.h` headers and private-code `.c` files
continue to be generated unchanged, so the existing `wl_*` C API remains
available during migration.

---

## CI additions required

- Add `libpugixml-dev` to the `meson.yml` apt install list (dependency of the
  scanner's `pugixml` fallback subproject).
- The wrap file will cause `meson setup` to fetch the scanner source.  If
  network access is restricted, vendor the source into `subprojects/` and omit
  the wrap `[wrap-git]` section.

---

## Acceptance criteria

- [ ] `subprojects/wayland-cxx-scanner.wrap` fetches and builds the tool.
- [ ] Every protocol that was previously generating a `.h` now also generates a
      `.hpp` via `--mode=client-header`.
- [ ] `#include <wl/proxy.hpp>` resolves for all waypp consumers.
- [ ] `cmake/WaylandCxxScanner.cmake` builds and finds the tool via `ExternalProject`.
- [ ] At least one waypp source file (`registrar.cc`) is ported to the C++23 API
      as a proof-of-concept; all existing tests still pass.
- [ ] Meson and CMake CI remain green throughout.
