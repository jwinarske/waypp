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

add_executable(gl-shadertoy shadertoy.cc)

target_compile_definitions(gl-shadertoy PRIVATE EGL_NO_X11 MESA_EGL_NO_X11_HEADERS)

target_include_directories(gl-shadertoy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party)

target_link_libraries(gl-shadertoy PRIVATE waypp wayland-gen PkgConfig::GLESv2 cxxopts::cxxopts)

if (IPO_SUPPORT_RESULT)
    set_property(TARGET gl-shadertoy PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

add_sanitizers(gl-shadertoy)
