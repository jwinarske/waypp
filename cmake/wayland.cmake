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
option(ENABLE_AGL_SHELL_CLIENT "Enable AGL shell Client" OFF)
option(ENABLE_IVI_SHELL_CLIENT "Enable ivi-shell Client" OFF)
option(ENABLE_DRM_LEASE_CLIENT "Enable DRM Lease Client" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client wayland-egl wayland-cursor xkbcommon)

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

macro(wayland_generate protocol_file output_file)
    add_custom_command(OUTPUT ${output_file}.h
            COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header < ${protocol_file} > ${output_file}.h
            DEPENDS ${protocol_file})
    list(APPEND WAYLAND_PROTOCOL_SOURCES ${output_file}.h)

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
include_directories(${CMAKE_CURRENT_BINARY_DIR}/protocols)

if (ENABLE_XDG_CLIENT)
    add_protocol(${WAYLAND_PROTOCOLS_BASE}/stable/xdg-shell/xdg-shell.xml)
endif ()
if (ENABLE_AGL_SHELL_CLIENT)
    add_protocol(${CMAKE_SOURCE_DIR}/third_party/agl/protocol/agl-shell.xml)
    add_protocol(${CMAKE_SOURCE_DIR}/third_party/agl/protocol/agl-shell-desktop.xml)
    add_protocol(${CMAKE_SOURCE_DIR}/third_party/agl/protocol/agl-screenshooter.xml)
endif ()
if (ENABLE_IVI_SHELL_CLIENT)
    add_protocol(${CMAKE_SOURCE_DIR}/third_party/weston/protocol/ivi-application.xml)
    add_protocol(${CMAKE_SOURCE_DIR}/third_party/weston/protocol/ivi-wm.xml)
endif ()
if (ENABLE_DRM_LEASE_CLIENT)
    add_protocol(${WAYLAND_PROTOCOLS_BASE}/staging/drm-lease/drm-lease-v1.xml)
endif ()

add_protocol(${WAYLAND_PROTOCOLS_BASE}/stable/presentation-time/presentation-time.xml)
message(STATUS "Presentation Time ..... ${HAS_WAYLAND_PROTOCOL_PRESENTATION_TIME}")

#
# Optional
#
add_protocol(${WAYLAND_PROTOCOLS_BASE}/staging/fractional-scale/fractional-scale-v1.xml)
message(STATUS "Fractional Scale ...... ${HAS_WAYLAND_PROTOCOL_FRACTIONAL_SCALE_V1}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/stable/viewporter/viewporter.xml)
message(STATUS "Viewporter ............ ${HAS_WAYLAND_PROTOCOL_VIEWPORTER}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/staging/tearing-control/tearing-control-v1.xml)
message(STATUS "Tearing Control ....... ${HAS_WAYLAND_PROTOCOL_TEARING_CONTROL_V1}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/xdg-output/xdg-output-unstable-v1.xml)
message(STATUS "XDG Output Manager .... ${HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1}")

add_protocol(${WAYLAND_PROTOCOLS_BASE}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml)
message(STATUS "XDG Decoration ........ ${HAS_WAYLAND_PROTOCOL_XDG_DECORATION_UNSTABLE_V1}")

#
# External
#
if (EXT_PROTOCOL)
    foreach (EXT ${EXT_PROTOCOL})
        add_protocol(${EXT})
    endforeach ()
endif ()

configure_file(cmake/wayland-protocols.h.in ${CMAKE_CURRENT_BINARY_DIR}/protocols/wayland-protocols.h)

add_library(wayland-gen STATIC ${WAYLAND_PROTOCOL_SOURCES})
target_link_libraries(wayland-gen PUBLIC PkgConfig::WAYLAND)

if (IPO_SUPPORT_RESULT)
    set_property(TARGET wayland-gen PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

add_sanitizers(wayland-gen)