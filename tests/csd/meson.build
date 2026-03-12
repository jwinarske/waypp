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

gtest_dep = dependency('gtest', main: false, required: true)
gtest_main_dep = dependency('gtest', main: true, required: true)

project_inc  = include_directories('../../include')
src_inc      = include_directories('../../src')

# ── test_csd_frame_extents ────────────────────────────────────────────────────

test(
  'csd.frame_extents',
  executable(
    'test_csd_frame_extents',
    'test_csd_frame_extents.cc',
    include_directories: project_inc,
    dependencies: gtest_main_dep,
  ),
)

# ── test_csd_hit_zones ────────────────────────────────────────────────────────

test(
  'csd.hit_zones',
  executable(
    'test_csd_hit_zones',
    'test_csd_hit_zones.cc',
    include_directories: [project_inc, src_inc],
    dependencies: [gtest_main_dep, waypp_dep, wayland_gen_dep],
  ),
)

# ── test_csd_shm_plugin_render ────────────────────────────────────────────────

test(
  'csd.shm_plugin_render',
  executable(
    'test_csd_shm_plugin_render',
    'test_csd_shm_plugin_render.cc',
    include_directories: [project_inc, src_inc],
    dependencies: [gtest_main_dep, waypp_dep, wayland_gen_dep],
  ),
)
