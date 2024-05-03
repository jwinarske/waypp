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
pkg_check_modules(GLESv2 IMPORTED_TARGET glesv2)

if (ENABLE_XDG_CLIENT)

    #
    # simple-egl
    #
    add_executable(simple-egl simple-egl.cc)
    target_include_directories(simple-egl PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

    target_compile_definitions(simple-egl PRIVATE EGL_NO_X11 MESA_EGL_NO_X11_HEADERS)

    target_link_libraries(simple-egl PRIVATE waypp wayland-gen PkgConfig::GLESv2 cxxopts::cxxopts)

    if (IPO_SUPPORT_RESULT)
        set_property(TARGET simple-egl PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif ()

    add_sanitizers(simple-egl)

    #
    # simple-shm
    #
    add_executable(simple-shm simple-shm.cc)
    target_include_directories(simple-shm PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

    target_link_libraries(simple-shm PRIVATE waypp wayland-gen cxxopts::cxxopts)

    if (IPO_SUPPORT_RESULT)
        set_property(TARGET simple-shm PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif ()

    add_sanitizers(simple-shm)

    #
    # presentation-shm
    #
    if (FALSE)
        add_executable(presentation-shm presentation-shm.cc)
        target_include_directories(presentation-shm PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

        target_link_libraries(presentation-shm PRIVATE waypp wayland-gen cxxopts::cxxopts)

        if (IPO_SUPPORT_RESULT)
            set_property(TARGET presentation-shm PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif ()

        add_sanitizers(presentation-shm)
    endif ()

    #
    # simple-ext-protocol
    #
    if (HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1)
        add_executable(simple-ext-protocol simple-ext-protocol.cc)
        target_include_directories(simple-ext-protocol PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

        target_link_libraries(simple-ext-protocol PRIVATE waypp wayland-gen cxxopts::cxxopts)

        if (IPO_SUPPORT_RESULT)
            set_property(TARGET simple-ext-protocol PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif ()

        add_sanitizers(simple-ext-protocol)
    endif ()

    #
    # agl-simple-shm
    #
    if (ENABLE_AGL_SHELL_CLIENT)
        add_executable(agl-simple-shm agl-simple-shm.cc)
        target_include_directories(agl-simple-shm PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

        target_link_libraries(agl-simple-shm PRIVATE waypp wayland-gen cxxopts::cxxopts)

        if (IPO_SUPPORT_RESULT)
            set_property(TARGET agl-simple-shm PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif ()

        add_sanitizers(agl-simple-shm)
    endif ()
endif ()
