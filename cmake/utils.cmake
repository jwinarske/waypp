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

function(get_cmake_properties)
    execute_process(COMMAND cmake --help-property-list OUTPUT_VARIABLE CMAKE_PROPERTY_LIST)
    # Convert command output into a CMake list
    string(REGEX REPLACE ";" "\\\\;" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
    string(REGEX REPLACE "\n" ";" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
    list(REMOVE_DUPLICATES CMAKE_PROPERTY_LIST)
endfunction()

function(print_properties)
    get_cmake_properties()
    message("CMAKE_PROPERTY_LIST = ${CMAKE_PROPERTY_LIST}")
endfunction()

function(print_target_properties target)
    if (NOT TARGET ${target})
        message(STATUS "There is no target named '${target}'")
        return()
    endif ()

    get_cmake_properties()

    foreach (property ${CMAKE_PROPERTY_LIST})
        string(REPLACE "<CONFIG>" "${CMAKE_BUILD_TYPE}" property ${property})

        # Simpler check for LOCATION property
        if (property MATCHES "(^LOCATION_|_LOCATION$|LOCATION)")
            continue()
        endif ()

        get_property(is_property_set TARGET ${target} PROPERTY ${property} SET)
        if (is_property_set)
            get_target_property(value ${target} ${property})
            message("${target} ${property} = ${value}")
        endif ()
    endforeach ()
endfunction()