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
        window_manager/agl_shell.cc
        window_manager/output.cc
        window_manager/registrar.cc
        window_manager/weston-capture.cc
        window_manager/window_manager.cc
        window_manager/xdg_output.cc
)

set(SEAT_SRC
        seat/seat.cc
        seat/keyboard.cc
        seat/pointer.cc
        seat/touch.cc
)

set(WINDOW_SRC
        window/anonymous_file.cc
        window/buffer.cc
        window/egl.cc
        window/feedback.cc
        window/window.cc
)

add_library(waypp
        ${WINDOW_MANAGER_SRC}
        ${SEAT_SRC}
        ${WINDOW_SRC}
        command.cc
)

if (BUILD_STANDALONE)
    target_sources(waypp PRIVATE
            logging.cc)
endif ()

if (ENABLE_XDG_CLIENT)
    target_sources(waypp PRIVATE
            window/xdg_toplevel.cc
            window_manager/xdg_window_manager.cc)
endif ()
if (ENABLE_AGL_CLIENT)
    target_sources(waypp PRIVATE window_manager/agl_shell.cc)
endif ()
if (ENABLE_IVI_SHELL_CLIENT)
    target_sources(waypp PRIVATE window_manager/ivi_window_manager.cc)
endif ()
if (ENABLE_DRM_LEASE_CLIENT)
    target_sources(waypp PRIVATE window_manager/drm_lease_device_v1.cc)
endif ()

target_include_directories(waypp PUBLIC .)

target_link_libraries(waypp PUBLIC
        wayland-gen
        PkgConfig::GLIB
        OpenGL::EGL
        spdlog
)

add_sanitizers(waypp)
