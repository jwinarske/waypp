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

include(glsl_compile)

set(SHADER_SOURCES
        shaders/src/buf.frag
        shaders/src/buf.frag
        shaders/src/buf.vert
        shaders/src/buf1.frag
        shaders/src/buf2.frag
        shaders/src/buf3.frag
        shaders/src/main.frag
        shaders/src/main.vert
)

add_executable(vk-shadertoy
        app.cc
        handlers.cc
        main.cc
        vulkan_backend.cc
)

append_glsl_to_target(vk-shadertoy "${SHADER_SOURCES}" 100)

target_link_libraries(vk-shadertoy PRIVATE waypp wayland-gen cxxopts::cxxopts)

if (IPO_SUPPORT_RESULT)
    set_property(TARGET vk-shadertoy PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

add_sanitizers(vk-shadertoy)
