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

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (ENABLE_EGL)
    pkg_check_modules(EGL REQUIRED IMPORTED_TARGET glesv2 egl)
endif ()

add_library(waypp

        seat/seat.cc
        seat/keyboard.cc
        seat/pointer.cc
        seat/touch.cc

        window/anonymous_file.cc
        window/buffer.cc
        window/feedback.cc
        window/window.cc

        window_manager/output.cc
        window_manager/registrar.cc
        window_manager/weston-capture.cc
        window_manager/window_manager.cc

        command.cc
)
target_include_directories(waypp PUBLIC ${LOGGING_INCLUDE_DIRS})
# print_target_properties(waypp)

if (ENABLE_EGL)
    target_sources(waypp PRIVATE window/egl.cc)
endif ()

if (ENABLE_XDG_CLIENT)
    target_sources(waypp PRIVATE
            window/xdg_toplevel.cc
            window_manager/xdg_output.cc
            window_manager/xdg_window_manager.cc
    )
endif ()
if (ENABLE_AGL_SHELL_CLIENT)
    target_sources(waypp PRIVATE window_manager/agl_shell.cc)
endif ()

target_compile_definitions(waypp PUBLIC
        MESA_EGL_NO_X11_HEADERS
        WL_EGL_PLATFORM
        EGL_NO_X11
)

target_link_libraries(waypp PUBLIC
        wayland-gen
        PkgConfig::GLIB
        spdlog
        Threads::Threads
        dl
        rt
)

if (ENABLE_EGL)
    target_link_libraries(waypp PUBLIC PkgConfig::EGL)
    target_compile_definitions(waypp PUBLIC EGL_NO_X11 MESA_EGL_NO_X11_HEADERS)
endif ()

add_sanitizers(waypp)

install(DIRECTORY ../include/waypp DESTINATION include)
install(TARGETS waypp DESTINATION lib)
