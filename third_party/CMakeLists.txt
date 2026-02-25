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