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

common_example_deps = [waypp_dep, wayland_gen_dep, cxxopts_dep, threads_dep]

# ── simple-shm ────────────────────────────────────────────────────────────────

executable(
  'simple-shm',
  'simple-shm.cc',
  dependencies: common_example_deps,
  install: true,
)

# ── simple-egl ────────────────────────────────────────────────────────────────

if enable_egl
  executable(
    'simple-egl',
    'simple-egl.cc',
    dependencies: common_example_deps + egl_deps,
    install: true,
  )

  subdir('gl-shadertoy')
endif

# ── view-manager ──────────────────────────────────────────────────────────────

subdir('view_manager')

# ── simple-csd ────────────────────────────────────────────────────────────────

if enable_csd and enable_xdg
  executable(
    'simple-csd',
    'simple-csd.cc',
    dependencies: common_example_deps,
    install: true,
  )
endif

# ── simple-ext-protocol ───────────────────────────────────────────────────────

if proto_flags['HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1']
  executable(
    'simple-ext-protocol',
    'simple-ext-protocol.cc',
    dependencies: common_example_deps,
    install: true,
  )
endif

# ── AGL examples ─────────────────────────────────────────────────────────────

if enable_agl
  executable(
    'agl-simple-shm',
    'agl-simple-shm.cc',
    dependencies: common_example_deps,
    install: true,
  )

  executable(
    'agl-capture',
    'agl-capture.cc',
    dependencies: common_example_deps,
    install: true,
  )
endif

# ── IVI examples ──────────────────────────────────────────────────────────────

if enable_ivi
  executable(
    'ivi-simple-shm',
    'ivi-simple-shm.cc',
    dependencies: common_example_deps,
    install: true,
  )
endif
