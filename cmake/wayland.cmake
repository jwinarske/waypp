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

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client wayland-egl wayland-cursor xkbcommon)

set(MIN_PROTOCOL_VER 1.13)
if (BUILD_BACKEND_WAYLAND_DRM)
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

set(WAYLAND_PROTOCOL_SOURCES)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/protocols)

wayland_generate(
        ${WAYLAND_PROTOCOLS_BASE}/stable/xdg-shell/xdg-shell.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocols/xdg-shell-client-protocol)

wayland_generate(
        ${CMAKE_SOURCE_DIR}/third_party/agl/protocol/agl-shell.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocols/agl-shell-client-protocol)
wayland_generate(
        ${CMAKE_SOURCE_DIR}/third_party/agl/protocol/agl-shell-desktop.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocols/agl-shell-desktop-client-protocol)
wayland_generate(
        ${CMAKE_SOURCE_DIR}/third_party/agl/protocol/agl-screenshooter.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocols/agl-screenshooter-client-protocol)

wayland_generate(
        ${CMAKE_SOURCE_DIR}/third_party/weston/protocol/ivi-application.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocols/ivi-application-client-protocol)
wayland_generate(
        ${CMAKE_SOURCE_DIR}/third_party/weston/protocol/ivi-wm.xml
        ${CMAKE_CURRENT_BINARY_DIR}/protocols/ivi-wm-client-protocol)

#
# Optional
#
set(WAYLAND_PROTOCOL_HAS_XDG_DECORATION OFF)
if (EXISTS ${WAYLAND_PROTOCOLS_BASE}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml)
    set(WAYLAND_PROTOCOL_HAS_XDG_DECORATION ON)
    wayland_generate(
            ${WAYLAND_PROTOCOLS_BASE}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
            ${CMAKE_CURRENT_BINARY_DIR}/protocols/xdg-decoration-unstable-client-protocol)
endif ()
message(STATUS "XDG Decoration ........ ${WAYLAND_PROTOCOL_HAS_XDG_DECORATION}")

set(WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE OFF)
if (EXISTS ${WAYLAND_PROTOCOLS_BASE}/staging/fractional-scale/fractional-scale-v1.xml)
    set(WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE ON)
    wayland_generate(
            ${WAYLAND_PROTOCOLS_BASE}/staging/fractional-scale/fractional-scale-v1.xml
            ${CMAKE_CURRENT_BINARY_DIR}/protocols/fractional-scale-v1-client-protocol)
endif ()
message(STATUS "Fractional Scale ...... ${WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE}")

set(WAYLAND_PROTOCOL_HAS_VIEWPORTER OFF)
if (EXISTS ${WAYLAND_PROTOCOLS_BASE}/stable/viewporter/viewporter.xml)
    set(WAYLAND_PROTOCOL_HAS_VIEWPORTER ON)
    wayland_generate(
            ${WAYLAND_PROTOCOLS_BASE}/stable/viewporter/viewporter.xml
            ${CMAKE_CURRENT_BINARY_DIR}/protocols/viewporter-client-protocol)
endif ()
message(STATUS "Viewporter ............ ${WAYLAND_PROTOCOL_HAS_VIEWPORTER}")

set(WAYLAND_PROTOCOL_HAS_TEARING_CONTROL OFF)
if (EXISTS ${WAYLAND_PROTOCOLS_BASE}/staging/tearing-control/tearing-control-v1.xml)
    set(WAYLAND_PROTOCOL_HAS_TEARING_CONTROL ON)
    wayland_generate(
            ${WAYLAND_PROTOCOLS_BASE}/staging/tearing-control/tearing-control-v1.xml
            ${CMAKE_CURRENT_BINARY_DIR}/protocols/tearing-control-v1-client-protocol)
endif ()
message(STATUS "Tearing Control ....... ${WAYLAND_PROTOCOL_HAS_TEARING_CONTROL}")

set(WAYLAND_PROTOCOL_HAS_PRESENTATION_TIME OFF)
if (EXISTS ${WAYLAND_PROTOCOLS_BASE}/stable/presentation-time/presentation-time.xml)
    set(WAYLAND_PROTOCOL_HAS_PRESENTATION_TIME ON)
    wayland_generate(
            ${WAYLAND_PROTOCOLS_BASE}/stable/presentation-time/presentation-time.xml
            ${CMAKE_CURRENT_BINARY_DIR}/protocols/presentation-time-client-protocol)
endif ()
message(STATUS "Presentation Time ..... ${WAYLAND_PROTOCOL_HAS_PRESENTATION_TIME}")

set(WAYLAND_PROTOCOL_HAS_DRM_LEASE OFF)
if (EXISTS ${WAYLAND_PROTOCOLS_BASE}/staging/drm-lease/drm-lease-v1.xml)
    set(WAYLAND_PROTOCOL_HAS_DRM_LEASE ON)
    wayland_generate(
            ${WAYLAND_PROTOCOLS_BASE}/staging/drm-lease/drm-lease-v1.xml
            ${CMAKE_CURRENT_BINARY_DIR}/protocols/drm-lease-v1-client-protocol)
endif ()
message(STATUS "DRM Lease ............. ${WAYLAND_PROTOCOL_HAS_DRM_LEASE}")


add_library(wayland-gen STATIC ${WAYLAND_PROTOCOL_SOURCES})
target_link_libraries(wayland-gen PUBLIC PkgConfig::WAYLAND)

if (ENABLE_XDG_CLIENT)
    target_compile_definitions(wayland-gen PUBLIC ENABLE_XDG_CLIENT)
endif ()
if (ENABLE_AGL_SHELL_CLIENT)
    target_compile_definitions(wayland-gen PUBLIC ENABLE_AGL_SHELL_CLIENT)
endif ()
if (ENABLE_IVI_SHELL_CLIENT)
    target_compile_definitions(wayland-gen PUBLIC ENABLE_IVI_SHELL_CLIENT)
endif ()

if (WAYLAND_PROTOCOL_HAS_XDG_DECORATION)
    target_compile_definitions(wayland-gen PUBLIC WAYLAND_PROTOCOL_HAS_XDG_DECORATION)
endif ()
if (WAYLAND_PROTOCOL_HAS_PRESENTATION_TIME)
    target_compile_definitions(wayland-gen PUBLIC WAYLAND_PROTOCOL_HAS_PRESENTATION_TIME)
endif ()
if (WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE)
    target_compile_definitions(wayland-gen PUBLIC WAYLAND_PROTOCOL_HAS_FRACTIONAL_SCALE)
endif ()
if (WAYLAND_PROTOCOL_HAS_VIEWPORTER)
    target_compile_definitions(wayland-gen PUBLIC WAYLAND_PROTOCOL_HAS_VIEWPORTER)
endif ()
if (WAYLAND_PROTOCOL_HAS_TEARING_CONTROL)
    target_compile_definitions(wayland-gen PUBLIC WAYLAND_PROTOCOL_HAS_TEARING_CONTROL)
endif ()

target_include_directories(wayland-gen PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

if (IPO_SUPPORT_RESULT)
    set_property(TARGET wayland-gen PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

add_sanitizers(wayland-gen)