#
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
#

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
find_package(OpenGL REQUIRED COMPONENTS EGL)

set(WINDOW_MANAGER_SRC
        window_manager/display.cc
        window_manager/output.cc
        window_manager/window_manager.cc
        window_manager/xdg_wm.cc)

set(SEAT_SRC
        seat/seat.cc
        seat/keyboard.cc
        seat/pointer.cc
        seat/cursor.cc
        seat/touch.cc)

set(WINDOW_SRC
        window/egl.cc
        window/window.cc
        window/window_egl.cc
        window/window_vulkan.cc)

add_library(waypp
        ${WINDOW_MANAGER_SRC}
        ${SEAT_SRC}
        ${WINDOW_SRC})

target_include_directories(waypp PUBLIC .)

target_link_libraries(waypp PUBLIC
        wayland-gen
        PkgConfig::GLIB
        OpenGL::EGL
)