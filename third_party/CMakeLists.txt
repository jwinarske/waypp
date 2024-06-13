#
# cxxopts
#
if (NOT TARGET cxxopts)
    add_subdirectory(cxxopts-3.2.1)
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
    target_include_directories(spdlog INTERFACE spdlog-1.13.0/include)
else ()

endif ()