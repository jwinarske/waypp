#
# Copyright 2020 Toyota Connected North America
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

#
# Client Options
#
option(ENABLE_XDG_CLIENT "Enable XDG Client" ON)
option(ENABLE_AGL_SHELL_CLIENT "Enable AGL shell Client" ON)
option(ENABLE_IVI_SHELL_CLIENT "Enable ivi-shell Client" OFF)
option(ENABLE_DRM_LEASE_CLIENT "Enable DRM Lease Client" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client wayland-cursor xkbcommon)
if (ENABLE_EGL)
    pkg_check_modules(WAYLAND_EGL REQUIRED IMPORTED_TARGET wayland-egl)
endif ()

include(CheckFunctionExists)
check_function_exists(memfd_create HAVE_MEMFD_CREATE)
check_function_exists(posix_fallocate HAVE_POSIX_FALLOCATE)
check_function_exists(mkostemp HAVE_MKOSTEMP)

set(MIN_PROTOCOL_VER 1.13)
if (ENABLE_DRM_LEASE)
    set(MIN_PROTOCOL_VER 1.22)
endif ()
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=${MIN_PROTOCOL_VER})
pkg_get_variable(WAYLAND_PROTOCOLS_BASE wayland-protocols pkgdatadir)

find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)
find_program(WAYLAND_CXX_SCANNER_EXECUTABLE NAMES wayland-cxx-scanner REQUIRED)

macro(wayland_generate protocol_file output_file)
    add_custom_command(OUTPUT ${output_file}.hpp
            COMMAND ${WAYLAND_CXX_SCANNER_EXECUTABLE} --mode=client-header ${protocol_file} ${output_file}.hpp
            DEPENDS ${protocol_file})
    list(APPEND WAYLAND_PROTOCOL_SOURCES ${output_file}.hpp)

    add_custom_command(OUTPUT ${output_file}.c
            COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code < ${protocol_file} > ${output_file}.c
            DEPENDS ${protocol_file})
    list(APPEND WAYLAND_PROTOCOL_SOURCES ${output_file}.c)
endmacro()

macro(add_protocol protocol_file)
    get_filename_component(PROTOCOL_PREFIX ${protocol_file} NAME_WLE)
    string(TOUPPER ${PROTOCOL_PREFIX} PROTOCOL_FILE_UPPER)
    string(REPLACE "-" "_" PROTOCOL_VAR ${PROTOCOL_FILE_UPPER})

    if (EXISTS ${protocol_file})
        wayland_generate(
                ${protocol_file}
                ${CMAKE_CURRENT_BINARY_DIR}/protocols/${PROTOCOL_PREFIX}-client-protocol)
        set(HAS_WAYLAND_PROTOCOL_${PROTOCOL_VAR} TRUE)
        list(APPEND LIST_WAYLAND_PROTOCOLS -DHAS_WAYLAND_PROTOCOL_${PROTOCOL_VAR})
    else ()
        set(HAS_WAYLAND_PROTOCOL_${PROTOCOL_VAR} FALSE)
    endif ()
endmacro()

set(WAYLAND_PROTOCOL_SOURCES)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/protocols)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/waypp)

#
# Local
#

if (ENABLE_XDG_CLIENT)
    add_protocol(${WAYLAND_PROTOCOLS_BASE}/stable/xdg-shell/xdg-shell.xml)
    add_protocol(${WAYLAND_PROTOCOLS_BASE}/staging/xdg-activation/xdg-activation-v1.xml)
    message(STATUS "XDG Activation ........ ${HAS_WAYLAND_PROTOCOL_XDG_ACTIVATION_V1}")
    add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/xdg-output/xdg-output-unstable-v1.xml)
    message(STATUS "XDG Output Manager .... ${HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1}")
    add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml)
    message(STATUS "XDG Decoration ........ ${HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1}")
endif ()

if (ENABLE_AGL_SHELL_CLIENT)
    add_protocol(${PROJECT_SOURCE_DIR}/third_party/agl/protocol/agl-shell.xml)
    add_protocol(${PROJECT_SOURCE_DIR}/third_party/agl/protocol/agl-shell-desktop.xml)
    add_protocol(${PROJECT_SOURCE_DIR}/third_party/agl/protocol/agl-screenshooter.xml)
endif ()

if (ENABLE_IVI_SHELL_CLIENT)
    add_protocol(${PROJECT_SOURCE_DIR}/third_party/weston/protocol/ivi-application.xml)
    message(STATUS "IVI Application ....... ${HAS_WAYLAND_PROTOCOL_IVI_APPLICATION}")
    add_protocol(${PROJECT_SOURCE_DIR}/third_party/weston/protocol/ivi-wm.xml)
    message(STATUS "IVI WM ................ ${HAS_WAYLAND_PROTOCOL_IVI_WM}")
endif ()

add_protocol(${PROJECT_SOURCE_DIR}/third_party/weston/protocol/weston-output-capture.xml)


#
# Stable
#
add_protocol(${WAYLAND_PROTOCOLS_BASE}/stable/linux-dmabuf/linux-dmabuf-v1.xml)
message(STATUS "Linux DMA Buffer ...... ${HAS_WAYLAND_PROTOCOL_LINUX_DMABUF_V1}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/stable/presentation-time/presentation-time.xml)
message(STATUS "Presentation Time ..... ${HAS_WAYLAND_PROTOCOL_PRESENTATION_TIME}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/stable/viewporter/viewporter.xml)
message(STATUS "Viewporter ............ ${HAS_WAYLAND_PROTOCOL_VIEWPORTER}")

#
# Staging
#
add_protocol(${WAYLAND_PROTOCOLS_BASE}/staging/cursor-shape/cursor-shape-v1.xml)
message(STATUS "Cursor Shape .......... ${HAS_WAYLAND_PROTOCOL_CURSOR_SHAPE_V1}")

if (ENABLE_DRM_LEASE_CLIENT)
    add_protocol(${WAYLAND_PROTOCOLS_BASE}/staging/drm-lease/drm-lease-v1.xml)
endif ()

add_protocol(${WAYLAND_PROTOCOLS_BASE}/staging/fractional-scale/fractional-scale-v1.xml)
message(STATUS "Fractional Scale ...... ${HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/staging/tearing-control/tearing-control-v1.xml)
message(STATUS "Tearing Control ....... ${HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1}")


#
# Unstable
#
add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/tablet/tablet-unstable-v1.xml)
message(STATUS "Tablet v1 ............. ${HAS_WAYLAND_PROTOCOL_TABLET_UNSTABLE_V1}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/tablet/tablet-unstable-v2.xml)
message(STATUS "Tablet v2 ............. ${HAS_WAYLAND_PROTOCOL_TABLET_UNSTABLE_V2}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml)
message(STATUS "Idle Inhibit .......... ${HAS_WAYLAND_PROTOCOL_IDLE_INHIBIT_UNSTABLE_V1}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml)
message(STATUS "Pointer Constraints ... ${HAS_WAYLAND_PROTOCOL_POINTER_CONSTRAINTS_UNSTABLE_V1}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml)
message(STATUS "Pointer Gestures ...... ${HAS_WAYLAND_PROTOCOL_POINTER_GESTURES_UNSTABLE_V1}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/relative-pointer/relative-pointer-unstable-v1.xml)
message(STATUS "Relative Pointer ...... ${HAS_WAYLAND_PROTOCOL_RELATIVE_POINTER_UNSTABLE_V1}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/primary-selection/primary-selection-unstable-v1.xml)
message(STATUS "Primary Selection  .... ${HAS_WAYLAND_PROTOCOL_PRIMARY_SELECTION_UNSTABLE_V1}")


#
# External
#
if (EXT_PROTOCOL)
    foreach (EXT ${EXT_PROTOCOL})
        add_protocol(${EXT})
    endforeach ()
endif ()

configure_file(cmake/waypp.h.in ${CMAKE_CURRENT_BINARY_DIR}/waypp/waypp.h)

add_library(wayland-gen STATIC ${WAYLAND_PROTOCOL_SOURCES})
target_link_libraries(wayland-gen PUBLIC PkgConfig::WAYLAND)
if (ENABLE_EGL)
    target_link_libraries(wayland-gen PUBLIC PkgConfig::WAYLAND_EGL)
endif ()
target_include_directories(wayland-gen PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/protocols
        ${CMAKE_CURRENT_BINARY_DIR}
)
target_include_directories(wayland-gen PUBLIC ${LOGGING_INCLUDE_DIRS})

add_sanitizers(wayland-gen)

# print_target_properties(wayland-gen)
