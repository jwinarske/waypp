# Copyright 2026 Joel Winarske
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project(
  'waypp',
  ['cpp', 'c'],
  version: '0.1.0',
  license: 'Apache-2.0',
  meson_version: '>= 1.1',
  default_options: [
    'cpp_std=c++23',
    'c_std=c11',
    'warning_level=2',
    'b_ndebug=if-release',
  ],
)

# ── Modules ───────────────────────────────────────────────────────────────────

fs = import('fs')
pkgconfig = import('pkgconfig')

# ── Options ───────────────────────────────────────────────────────────────────

enable_egl     = get_option('enable_egl')
enable_csd     = get_option('enable_csd')
enable_xdg     = get_option('enable_xdg_client')
enable_agl     = get_option('enable_agl_shell')
enable_ivi     = get_option('enable_ivi_shell')
enable_drm     = get_option('enable_drm_lease')
build_examples = get_option('build_examples')
build_tests    = get_option('build_tests')

# ── Global compile flags ──────────────────────────────────────────────────────

add_global_arguments(
  '-DMESA_EGL_NO_X11_HEADERS',
  '-DWL_EGL_PLATFORM',
  '-DEGL_NO_X11',
  language: ['c', 'cpp'],
)

# ── Compiler feature checks ───────────────────────────────────────────────────

cc = meson.get_compiler('c')

have_memfd_create    = cc.has_function('memfd_create')
have_posix_fallocate = cc.has_function('posix_fallocate')
have_mkostemp        = cc.has_function('mkostemp')

# ── Dependencies ──────────────────────────────────────────────────────────────

threads_dep    = dependency('threads')
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
xkbcommon      = dependency('xkbcommon')
glib           = dependency('glib-2.0')

egl_deps = []
if enable_egl
  egl_deps = [
    dependency('wayland-egl'),
    dependency('egl'),
    dependency('glesv2'),
  ]
endif

# ── Wayland protocols ─────────────────────────────────────────────────────────

min_proto_ver = enable_drm ? '1.22' : '1.13'
wp            = dependency('wayland-protocols', version: '>= ' + min_proto_ver)
wp_dir        = wp.get_variable(pkgconfig: 'pkgdatadir')

wl_scanner = find_program('wayland-scanner')

# ── wayland-cxx-scanner (host tool) ──────────────────────────────────────────
#
# wayland-cxx-scanner is a host build tool installed separately in CI
# (see .github/workflows/meson.yml).  It is found the same way as
# wayland-scanner — via find_program — and used only to generate C++23
# protocol proxy headers (.hpp) from Wayland XML files at build time.
# No framework library dependency is required here; only the generated
# header files are consumed by the waypp build.

wl_cxx_scanner_exe = find_program('wayland-cxx-scanner')

# ── Third-party header-only libraries ────────────────────────────────────────

cxxopts_dep = declare_dependency(
  include_directories: include_directories(
    'third_party/cxxopts-3.2.1/include',
    is_system: true,
  ),
)

spdlog_dep = declare_dependency(
  include_directories: include_directories(
    'third_party/spdlog-1.15.0/include',
    is_system: true,
  ),
)

# ── Protocol generation ───────────────────────────────────────────────────────
#
# Each entry in proto_list is a 3-element array:
#   [ xml_path (absolute string),  output_stem,  flag_key ]
#
# Protocols are only generated when the XML file exists on disk.
# The flag_key is collected into active_proto_flags and used to populate
# the configuration_data for waypp.h.

proto_list = []

# Stable ──────────────────────────────────────────────────────────────────────
proto_list += [[
  wp_dir / 'stable/linux-dmabuf/linux-dmabuf-v1.xml',
  'linux-dmabuf-v1-client-protocol',
  'HAS_WAYLAND_PROTOCOL_LINUX_DMABUF_V1',
]]
proto_list += [[
  wp_dir / 'stable/presentation-time/presentation-time.xml',
  'presentation-time-client-protocol',
  'HAS_WAYLAND_PROTOCOL_PRESENTATION_TIME',
]]
proto_list += [[
  wp_dir / 'stable/viewporter/viewporter.xml',
  'viewporter-client-protocol',
  'HAS_WAYLAND_PROTOCOL_VIEWPORTER',
]]

# XDG ─────────────────────────────────────────────────────────────────────────
if enable_xdg
  proto_list += [[
    wp_dir / 'stable/xdg-shell/xdg-shell.xml',
    'xdg-shell-client-protocol',
    'HAS_WAYLAND_PROTOCOL_XDG_SHELL',
  ]]
  proto_list += [[
    wp_dir / 'staging/xdg-activation/xdg-activation-v1.xml',
    'xdg-activation-v1-client-protocol',
    'HAS_WAYLAND_PROTOCOL_XDG_ACTIVATION_V1',
  ]]
  proto_list += [[
    wp_dir / 'unstable/xdg-output/xdg-output-unstable-v1.xml',
    'xdg-output-unstable-v1-client-protocol',
    'HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1',
  ]]
  proto_list += [[
    wp_dir / 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml',
    'xdg-decoration-unstable-v1-client-protocol',
    'HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1',
  ]]
endif

# Staging ─────────────────────────────────────────────────────────────────────
proto_list += [[
  wp_dir / 'staging/cursor-shape/cursor-shape-v1.xml',
  'cursor-shape-v1-client-protocol',
  'HAS_WAYLAND_PROTOCOL_CURSOR_SHAPE_V1',
]]
proto_list += [[
  wp_dir / 'staging/fractional-scale/fractional-scale-v1.xml',
  'fractional-scale-v1-client-protocol',
  'HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1',
]]
proto_list += [[
  wp_dir / 'staging/tearing-control/tearing-control-v1.xml',
  'tearing-control-v1-client-protocol',
  'HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1',
]]

if enable_drm
  proto_list += [[
    wp_dir / 'staging/drm-lease/drm-lease-v1.xml',
    'drm-lease-v1-client-protocol',
    'HAS_WAYLAND_PROTOCOL_DRM_LEASE_V1',
  ]]
endif

# Unstable ────────────────────────────────────────────────────────────────────
proto_list += [[
  wp_dir / 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml',
  'idle-inhibit-unstable-v1-client-protocol',
  'HAS_WAYLAND_PROTOCOL_IDLE_INHIBIT_UNSTABLE_V1',
]]
proto_list += [[
  wp_dir / 'unstable/pointer-constraints/pointer-constraints-unstable-v1.xml',
  'pointer-constraints-unstable-v1-client-protocol',
  'HAS_WAYLAND_PROTOCOL_POINTER_CONSTRAINTS_UNSTABLE_V1',
]]
proto_list += [[
  wp_dir / 'unstable/pointer-gestures/pointer-gestures-unstable-v1.xml',
  'pointer-gestures-unstable-v1-client-protocol',
  'HAS_WAYLAND_PROTOCOL_POINTER_GESTURES_UNSTABLE_V1',
]]
proto_list += [[
  wp_dir / 'unstable/relative-pointer/relative-pointer-unstable-v1.xml',
  'relative-pointer-unstable-v1-client-protocol',
  'HAS_WAYLAND_PROTOCOL_RELATIVE_POINTER_UNSTABLE_V1',
]]
proto_list += [[
  wp_dir / 'unstable/primary-selection/primary-selection-unstable-v1.xml',
  'primary-selection-unstable-v1-client-protocol',
  'HAS_WAYLAND_PROTOCOL_PRIMARY_SELECTION_UNSTABLE_V1',
]]
proto_list += [[
  wp_dir / 'unstable/tablet/tablet-unstable-v1.xml',
  'tablet-unstable-v1-client-protocol',
  'HAS_WAYLAND_PROTOCOL_TABLET_UNSTABLE_V1',
]]
proto_list += [[
  wp_dir / 'unstable/tablet/tablet-unstable-v2.xml',
  'tablet-unstable-v2-client-protocol',
  'HAS_WAYLAND_PROTOCOL_TABLET_UNSTABLE_V2',
]]

# AGL ─────────────────────────────────────────────────────────────────────────
if enable_agl
  proto_list += [[
    meson.current_source_dir() / 'third_party/agl/protocol/agl-shell.xml',
    'agl-shell-client-protocol',
    'HAS_WAYLAND_PROTOCOL_AGL_SHELL',
  ]]
  proto_list += [[
    meson.current_source_dir() / 'third_party/agl/protocol/agl-shell-desktop.xml',
    'agl-shell-desktop-client-protocol',
    'HAS_WAYLAND_PROTOCOL_AGL_SHELL_DESKTOP',
  ]]
  proto_list += [[
    meson.current_source_dir() / 'third_party/agl/protocol/agl-screenshooter.xml',
    'agl-screenshooter-client-protocol',
    'HAS_WAYLAND_PROTOCOL_AGL_SCREENSHOOTER',
  ]]
endif

# IVI / Weston ────────────────────────────────────────────────────────────────
if enable_ivi
  proto_list += [[
    meson.current_source_dir() / 'third_party/weston/protocol/ivi-application.xml',
    'ivi-application-client-protocol',
    'HAS_WAYLAND_PROTOCOL_IVI_APPLICATION',
  ]]
  proto_list += [[
    meson.current_source_dir() / 'third_party/weston/protocol/ivi-wm.xml',
    'ivi-wm-client-protocol',
    'HAS_WAYLAND_PROTOCOL_IVI_WM',
  ]]
endif

proto_list += [[
  meson.current_source_dir() / 'third_party/weston/protocol/weston-output-capture.xml',
  'weston-output-capture-client-protocol',
  'HAS_WAYLAND_PROTOCOL_WESTON_OUTPUT_CAPTURE',
]]

# ── Generate protocol files ───────────────────────────────────────────────────
#
# Iterate the list: for each XML that exists, produce:
#   • a C++23 client proxy header (.hpp) via wayland-cxx-scanner  ← only header
#   • a private-code C file (.c) via wayland-scanner               ← wl_interface ABI data
#
# C client-headers (.h from "wayland-scanner client-header") are NOT generated.
# The waypp source must use the C++23 proxy API from the generated .hpp files.

wayland_proto_hpp  = []  # custom_target objects that produce .hpp files
wayland_proto_c    = []  # custom_target objects that produce .c files
active_proto_flags = []  # list of flag key strings that were activated

foreach p : proto_list
  xml  = p[0]
  stem = p[1]
  flag = p[2]

  if fs.is_file(xml)
    hpp = custom_target(
      stem + '-hpp',
      output:  stem + '.hpp',
      command: [wl_cxx_scanner_exe, '--mode=client-header', xml, '@OUTPUT@'],
    )
    c = custom_target(
      stem + '-c',
      output:  stem + '.c',
      command: [wl_scanner, 'private-code', xml, '@OUTPUT@'],
    )
    wayland_proto_hpp   += [hpp]
    wayland_proto_c     += [c]
    active_proto_flags  += [flag]
  endif
endforeach

# ── wayland-gen static library ────────────────────────────────────────────────
#
# The .c custom_targets are compiled as C (wl_interface ABI data).
# The .hpp custom_targets are listed as sources so Meson guarantees they are
# generated before compilation starts and automatically adds the build-directory
# to include paths — allowing  #include "xdg-shell-client-protocol.hpp"  etc.

wayland_gen_lib = static_library(
  'wayland-gen',
  sources:          wayland_proto_c + wayland_proto_hpp,
  dependencies:     wayland_client,
  override_options: ['c_std=c11'],
  pic:              true,
)

# ── Config header ─────────────────────────────────────────────────────────────
#
# configure_file output must not contain path separators (Meson 1.3 restriction).
# Delegate to include/waypp/meson.build so configure_file(output: 'waypp.h')
# places the result at  <build>/include/waypp/waypp.h.
#
# Crucially, configure_file runs at configure time and creates the output
# directory immediately, so the include_directories(...) call that follows can
# safely reference  <build>/include/  as an absolute path.

all_proto_flag_keys = [
  'HAS_WAYLAND_PROTOCOL_XDG_SHELL',
  'HAS_WAYLAND_PROTOCOL_AGL_SHELL',
  'HAS_WAYLAND_PROTOCOL_AGL_SHELL_DESKTOP',
  'HAS_WAYLAND_PROTOCOL_AGL_SCREENSHOOTER',
  'HAS_WAYLAND_PROTOCOL_IVI_APPLICATION',
  'HAS_WAYLAND_PROTOCOL_IVI_WM',
  'HAS_WAYLAND_PROTOCOL_DRM_LEASE_V1',
  'HAS_WAYLAND_PROTOCOL_PRESENTATION_TIME',
  'HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1',
  'HAS_WAYLAND_PROTOCOL_VIEWPORTER',
  'HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1',
  'HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1',
  'HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1',
  'HAS_WAYLAND_PROTOCOL_XDG_ACTIVATION_V1',
  'HAS_WAYLAND_PROTOCOL_LINUX_DMABUF_V1',
  'HAS_WAYLAND_PROTOCOL_WESTON_OUTPUT_CAPTURE',
  'HAS_WAYLAND_PROTOCOL_CURSOR_SHAPE_V1',
  'HAS_WAYLAND_PROTOCOL_POINTER_CONSTRAINTS_UNSTABLE_V1',
  'HAS_WAYLAND_PROTOCOL_POINTER_GESTURES_UNSTABLE_V1',
  'HAS_WAYLAND_PROTOCOL_RELATIVE_POINTER_UNSTABLE_V1',
  'HAS_WAYLAND_PROTOCOL_IDLE_INHIBIT_UNSTABLE_V1',
  'HAS_WAYLAND_PROTOCOL_PRIMARY_SELECTION_UNSTABLE_V1',
  'HAS_WAYLAND_PROTOCOL_TABLET_UNSTABLE_V1',
  'HAS_WAYLAND_PROTOCOL_TABLET_UNSTABLE_V2',
]

conf = configuration_data()
conf.set('ENABLE_EGL',              enable_egl  ? 1 : 0)
conf.set('ENABLE_CSD',              enable_csd  ? 1 : 0)
conf.set('ENABLE_XDG_CLIENT',       enable_xdg  ? 1 : 0)
conf.set('ENABLE_AGL_SHELL_CLIENT', enable_agl  ? 1 : 0)
conf.set('ENABLE_IVI_SHELL_CLIENT', enable_ivi  ? 1 : 0)
conf.set('ENABLE_DRM_LEASE_CLIENT', enable_drm  ? 1 : 0)
conf.set('HAVE_MEMFD_CREATE',       have_memfd_create    ? 1 : 0)
conf.set('HAVE_POSIX_FALLOCATE',    have_posix_fallocate ? 1 : 0)
conf.set('HAVE_MKOSTEMP',           have_mkostemp        ? 1 : 0)
conf.set('BUILD_WAYPP_STANDALONE',  1)

foreach key : all_proto_flag_keys
  conf.set(key, key in active_proto_flags ? 1 : 0)
endforeach

# conf is visible in the subdir scope; configure_file runs there and creates
# <build>/include/waypp/ before the include_directories call below.
subdir('include/waypp')

# ── Build-tree include roots ──────────────────────────────────────────────────
#
# <build>/         — generated protocol headers (*-client-protocol.hpp)
# <build>/include/ — generated config header   (waypp/waypp.h)
#
# subdir('include/waypp') above has already created <build>/include/waypp/ so
# the absolute-path check in include_directories succeeds here.
build_root_inc = include_directories(
  meson.project_build_root(),
  meson.project_build_root() / 'include',
)

wayland_gen_dep = declare_dependency(
  link_with:           wayland_gen_lib,
  include_directories: [include_directories('include'), build_root_inc],
  dependencies:        wayland_client,
)

# ── Subdirectories ────────────────────────────────────────────────────────────

subdir('src')

if build_examples
  subdir('examples')
endif

if build_tests
  subdir('tests')
endif
