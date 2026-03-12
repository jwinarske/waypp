# Copyright 2024 Joel Winarske
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

waypp_compile_args = [
  '-DMESA_EGL_NO_X11_HEADERS',
  '-DWL_EGL_PLATFORM',
  '-DEGL_NO_X11',
]

waypp_deps = [
  wayland_gen_dep,
  glib,
  spdlog_dep,
  threads_dep,
]

if enable_egl
  waypp_sources += files('window/egl.cc')
  waypp_deps += egl_deps
  waypp_compile_args += ['-DEGL_NO_X11', '-DMESA_EGL_NO_X11_HEADERS']
endif

if enable_xdg
  waypp_sources += files(
    'window/xdg_toplevel.cc',
    'window_manager/xdg_output.cc',
    'window_manager/xdg_window_manager.cc',
  )
endif

if enable_csd and enable_xdg
  waypp_sources += files(
    'window/csd_frame.cc',
    'window/csd_shm_plugin.cc',
  )
  waypp_compile_args += ['-DENABLE_CSD=1']
endif

if enable_agl
  waypp_sources += files('window_manager/agl_shell.cc')
endif

if enable_ivi
  waypp_sources += files(
    'window_manager/ivi_window_manager.cc',
    'window_manager/ivi_wm.cc',
    'window/ivi_surface.cc',
  )
  waypp_compile_args += ['-DENABLE_IVI_SHELL_CLIENT=1']
endif

waypp_lib = library(
  'waypp',
  waypp_sources,
  include_directories: [
    include_directories('../include'),
    include_directories('../include/waypp'),
    waypp_conf_inc,
  ],
  cpp_args: waypp_compile_args,
  dependencies: waypp_deps,
  install: true,
)

waypp_dep = declare_dependency(
  link_with: waypp_lib,
  include_directories: [
    include_directories('../include'),
    include_directories('../include/waypp'),
    waypp_conf_inc,
  ],
  dependencies: waypp_deps,
)

install_headers(
  '../include/waypp/config.h',
  '../include/waypp/window_manager/agl_shell.h',
  '../include/waypp/window_manager/ivi_window_manager.h',
  '../include/waypp/window_manager/ivi_wm.h',
  '../include/waypp/window_manager/output.h',
  '../include/waypp/window_manager/registrar.h',
  '../include/waypp/window_manager/weston-capture.h',
  '../include/waypp/window_manager/window_manager.h',
  '../include/waypp/window_manager/window_manager_factory.h',
  '../include/waypp/window_manager/xdg_output.h',
  '../include/waypp/window_manager/xdg_window_manager.h',
  '../include/waypp/seat/keyboard.h',
  '../include/waypp/seat/pointer.h',
  '../include/waypp/seat/seat.h',
  '../include/waypp/seat/touch.h',
  '../include/waypp/window/buffer.h',
  '../include/waypp/window/csd_frame.h',
  '../include/waypp/window/csd_frame_extents.h',
  '../include/waypp/window/csd_hit_zone.h',
  '../include/waypp/window/csd_plugin.h',
  '../include/waypp/window/egl.h',
  '../include/waypp/window/feedback.h',
  '../include/waypp/window/ivi_surface.h',
  '../include/waypp/window/window.h',
  '../include/waypp/window/xdg_toplevel.h',
  subdir: 'waypp',
)

pkgconfig.generate(
  waypp_lib,
  name: 'waypp',
  description: 'Wayland C++ Library',
  version: meson.project_version(),
)
