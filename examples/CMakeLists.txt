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

#
# simple-egl
#
if (OFF)
    add_executable(simple-egl simple-egl.cc)
    target_include_directories(simple-egl PRIVATE ${CMAKE_SOURCE_DIR}/include)

    target_compile_definitions(simple-egl PRIVATE EGL_NO_X11 MESA_EGL_NO_X11_HEADERS)

    target_link_libraries(simple-egl PRIVATE waypp PkgConfig::GLESv2 cxxopts::cxxopts)

    if (IPO_SUPPORT_RESULT)
        set_property(TARGET simple-egl PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif ()

    add_sanitizers(simple-egl)
endif ()

#
# simple-shm
#
add_executable(simple-shm simple-shm.cc)
target_include_directories(simple-shm PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(simple-shm PRIVATE waypp cxxopts::cxxopts)

if (IPO_SUPPORT_RESULT)
    set_property(TARGET simple-shm PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

add_sanitizers(simple-shm)
