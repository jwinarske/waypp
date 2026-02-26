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


if (ENABLE_EGL)

    #
    # simple-egl
    #
    add_executable(simple-egl simple-egl.cc)

    target_link_libraries(simple-egl PRIVATE waypp wayland-gen cxxopts::cxxopts toolchain::toolchain)

    add_sanitizers(simple-egl)

    install(TARGETS simple-egl DESTINATION bin)

    #
    # OpenGL
    #
    add_subdirectory(gl-shadertoy)

endif ()

#
# view-manager
#
add_subdirectory(view_manager)

#
# simple-shm
#
add_executable(simple-shm simple-shm.cc)

target_link_libraries(simple-shm PRIVATE waypp wayland-gen cxxopts::cxxopts toolchain::toolchain)

add_sanitizers(simple-shm)

install(TARGETS simple-shm DESTINATION bin)

#
# simple-csd — client-side decoration demo (requires ENABLE_CSD + ENABLE_XDG_CLIENT)
#
if (ENABLE_CSD AND ENABLE_XDG_CLIENT)
    add_executable(simple-csd simple-csd.cc)

    target_link_libraries(simple-csd PRIVATE waypp wayland-gen cxxopts::cxxopts toolchain::toolchain)

    add_sanitizers(simple-csd)

    install(TARGETS simple-csd DESTINATION bin)
endif ()

#
# presentation-shm
#
if (FALSE)
    add_executable(presentation-shm presentation-shm.cc)

    target_link_libraries(presentation-shm PRIVATE waypp wayland-gen cxxopts::cxxopts toolchain::toolchain)

    add_sanitizers(presentation-shm)

    install(TARGETS presentation-shm DESTINATION bin)
endif ()

#
# simple-ext-protocol
#
if (HAS_WAYLAND_PROTOCOL_XDG_OUTPUT_UNSTABLE_V1)
    add_executable(simple-ext-protocol simple-ext-protocol.cc)

    target_link_libraries(simple-ext-protocol PRIVATE waypp wayland-gen cxxopts::cxxopts toolchain::toolchain)

    add_sanitizers(simple-ext-protocol)

    install(TARGETS simple-ext-protocol DESTINATION bin)
endif ()

#
# AGL specific apps
#
if (ENABLE_AGL_SHELL_CLIENT)

    #
    # agl-simple-shm
    #
    add_executable(agl-simple-shm agl-simple-shm.cc)

    target_link_libraries(agl-simple-shm PRIVATE waypp wayland-gen cxxopts::cxxopts toolchain::toolchain)

    add_sanitizers(agl-simple-shm)

    install(TARGETS agl-simple-shm DESTINATION bin)


    add_executable(agl-capture agl-capture.cc agl-capture.cc)

    target_link_libraries(agl-capture PRIVATE waypp wayland-gen cxxopts::cxxopts toolchain::toolchain)

    add_sanitizers(agl-capture)

    install(TARGETS agl-capture DESTINATION bin)
endif ()

#
# Vulkan
#
if (ENABLE_VULKAN)
    find_package(Vulkan)

    if (VULKAN_FOUND)
        add_subdirectory(vk-shadertoy)
    endif ()
endif ()