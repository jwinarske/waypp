#
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
#

# ---------------------------------------------------------------------------
# CSD unit tests — run without a compositor (no Wayland objects required)
# ---------------------------------------------------------------------------

# test_csd_frame_extents — pure arithmetic, zero deps
add_executable(test_csd_frame_extents
        test_csd_frame_extents.cc
)
target_include_directories(test_csd_frame_extents PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(test_csd_frame_extents PRIVATE gtest_main)
add_sanitizers(test_csd_frame_extents)
add_test(NAME csd.frame_extents COMMAND test_csd_frame_extents)

# test_csd_hit_zones — verifies CsdShmPlugin::hit_test() zone classification
# using mock surface pointers; no compositor needed.
add_executable(test_csd_hit_zones
        test_csd_hit_zones.cc
)
target_include_directories(test_csd_hit_zones PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(test_csd_hit_zones PRIVATE gtest_main waypp wayland-gen)
add_sanitizers(test_csd_hit_zones)
add_test(NAME csd.hit_zones COMMAND test_csd_hit_zones)

# test_csd_shm_plugin_render — pixel-level rendering verification on a
# manually-allocated SHM region; no compositor needed.
add_executable(test_csd_shm_plugin_render
        test_csd_shm_plugin_render.cc
)
target_include_directories(test_csd_shm_plugin_render PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(test_csd_shm_plugin_render PRIVATE gtest_main waypp wayland-gen)
add_sanitizers(test_csd_shm_plugin_render)
add_test(NAME csd.shm_plugin_render COMMAND test_csd_shm_plugin_render)

