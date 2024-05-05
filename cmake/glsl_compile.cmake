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

find_program(GLSL_VALIDATOR glslangValidator REQUIRED)

macro(append_glsl_to_target target sources version)

    foreach (GLSL ${sources})

        get_filename_component(FILE_NAME ${GLSL} NAME)

        set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")

        if (NOT EXISTS "${GLSL}")
            set(GLSL "${CMAKE_CURRENT_SOURCE_DIR}/${GLSL}")
        endif ()

        add_custom_command(
                OUTPUT ${SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
                COMMAND ${GLSL_VALIDATOR} -V${version} ${GLSL} -o ${SPIRV}
                DEPENDS ${GLSL}
        )

        list(APPEND SPIRV_BINARY_FILES ${SPIRV})

    endforeach ()

    add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})

    add_dependencies(${target} Shaders)

    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/shaders/"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:${target}>/shaders"
    )
endmacro()
