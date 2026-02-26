#
# cxxopts
#
if (NOT TARGET cxxopts)
    add_library(cxxopts INTERFACE)
    target_compile_options(cxxopts INTERFACE -isystem${CMAKE_CURRENT_SOURCE_DIR}/cxxopts-3.2.1/include)
    add_library(cxxopts::cxxopts ALIAS cxxopts)
endif ()

#
# Speedlog
#
if (BUILD_WAYPP_STANDALONE)
    set(LOG_NO_EXCEPTIONS ON)
    set(LOG_NO_THREAD_ID ON)
    # set(LOG_BUILD_PIC ON)
    set(LOG_SANITIZE_ADDRESS ${SANITIZE_ADDRESS})
    add_library(spdlog INTERFACE)
    target_include_directories(spdlog INTERFACE spdlog-1.15.0/include)
else ()

endif ()

#
# GoogleTest (only when unit tests are requested)
#
if (WAYPP_BUILD_UNIT_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK   OFF CACHE BOOL "" FORCE)
    add_subdirectory(googletest)

    # Suppress project-level strict flags that break googletest internals.
    foreach (_gt_target gtest gtest_main)
        if (TARGET ${_gt_target})
            target_compile_options(${_gt_target} PRIVATE
                    -Wno-error
                    -Wno-sign-conversion
                    -Wno-conversion
                    -Wno-undef
                    -include stdint.h
            )
        endif ()
    endforeach ()
endif ()

