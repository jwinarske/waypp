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

waypp_sources = files(
  'seat/seat.cc',
  'seat/keyboard.cc',
  'seat/pointer.cc',
  'seat/touch.cc',

  'window/anonymous_file.cc',
  'window/buffer.cc',
  'window/feedback.cc',
  'window/window.cc',

  'window_manager/output.cc',
  'window_manager/registrar.cc',
  'window_manager/weston-capture.cc',
  'window_manager/window_manager.cc',
  'window_manager/window_manager_factory.cc',

  'command.cc',
)

waypp_cpp_args = []
waypp_deps = [wayland_gen_dep, wayland_cursor, xkbcommon, glib, spdlog_dep, threads_dep]

if enable_egl
  waypp_sources += files('window/egl.cc')
  waypp_deps    += egl_deps
endif

if enable_xdg
  waypp_sources += files(
    'window/xdg_toplevel.cc',
    'window_manager/xdg_output.cc',
    'window_manager/xdg_window_manager.cc',
  )
  waypp_cpp_args += ['-DENABLE_XDG_CLIENT=1']
endif

if enable_csd and enable_xdg
  waypp_sources  += files('window/csd_frame.cc', 'window/csd_shm_plugin.cc')
  waypp_cpp_args += ['-DENABLE_CSD=1']
endif

if enable_agl
  waypp_sources  += files('window_manager/agl_shell.cc')
  waypp_cpp_args += ['-DENABLE_AGL_SHELL_CLIENT=1']
endif

if enable_ivi
  waypp_sources += files(
    'window_manager/ivi_window_manager.cc',
    'window_manager/ivi_wm.cc',
    'window/ivi_surface.cc',
  )
  waypp_cpp_args += ['-DENABLE_IVI_SHELL_CLIENT=1']
endif

# build_root_inc is defined in the root meson.build and covers both the
# generated protocol headers and the generated waypp/waypp.h config header.
# include/waypp is added for logging/logging.h (mirrors CMake LOGGING_INCLUDE_DIRS).
waypp_inc = [
  include_directories('../include'),
  include_directories('../include/waypp'),
  build_root_inc,
]

waypp_lib = library(
  'waypp',
  waypp_sources,
  include_directories: waypp_inc,
  cpp_args:            waypp_cpp_args,
  dependencies:        waypp_deps,
  install:             true,
)

waypp_dep = declare_dependency(
  link_with:           waypp_lib,
  include_directories: waypp_inc,
  dependencies:        waypp_deps,
)

install_subdir(
  '../include/waypp',
  install_dir: 'include',
  strip_directory: false,
)

pkgconfig.generate(
  waypp_lib,
  name:        'waypp',
  description: 'Wayland C++ Library',
  version:     meson.project_version(),
)
