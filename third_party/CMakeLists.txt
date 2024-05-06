#
# cxxopts
#
if (NOT TARGET cxxopts)
    add_subdirectory(cxxopts-3.2.1)
endif ()

#
# Speedlog
#
if (NOT TARGET spdlog)
    set(SPDLOG_NO_EXCEPTIONS ON)
    set(SPDLOG_NO_THREAD_ID ON)
    # set(SPDLOG_BUILD_PIC ON)
    set(SPDLOG_SANITIZE_ADDRESS ${SANITIZE_ADDRESS})
    add_library(spdlog INTERFACE)
    target_include_directories(spdlog INTERFACE spdlog-1.13.0/include)
endif ()