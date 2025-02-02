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

#
# Configure options
#
option(ENABLE_XDG_CLIENT "Enable XDG Client" ON)
option(ENABLE_AGL_SHELL_CLIENT "Enable AGL Shell" OFF)

option(ENABLE_EGL "Enable EGL dependency" ON)

#
# Link Time Optimization
#
option(ENABLE_LTO "Link Time Optimization" OFF)
MESSAGE(STATUS "Link Time Optimizaiton.. ${ENABLE_LTO}")

#
# Examples
#
option(BUILD_EXAMPLES "Build Examples" OFF)
MESSAGE(STATUS "Build Examples ......... ${BUILD_EXAMPLES}")

#
# Unit Tests
#
option(BUILD_UNIT_TESTS "Build Unit Tests" OFF)
MESSAGE(STATUS "Build Unit Tests ....... ${BUILD_UNIT_TESTS}")

#
# Sanitizers
#
find_package(Sanitizers)

#
# Standalone build
#
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(LOGGING_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include/waypp)
    set(BUILD_WAYPP_STANDALONE ON)
else()
    set(BUILD_WAYPP_STANDALONE OFF)
endif()
