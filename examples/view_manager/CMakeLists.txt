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

add_executable(view-manager
        main.cc
        app.cc
        view_manager_wayland.cc
        view_wayland.cc
)

target_include_directories(view-manager PRIVATE .)

target_link_libraries(view-manager PRIVATE waypp wayland-gen cxxopts::cxxopts)

if (IPO_SUPPORT_RESULT)
    set_property(TARGET view-manager PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

add_sanitizers(view-manager)

install(TARGETS view-manager DESTINATION bin)
