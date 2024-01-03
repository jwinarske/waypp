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

# Common settings
set(CPACK_GENERATOR "TGZ;RPM;DEB")
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/_packages")

# Package specific settings
set(CPACK_PACKAGE_VENDOR "Joel Winarske")
set(CPACK_PACKAGE_CONTACT "joel.winarske@gmail.com")
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(PACKAGE_NAME_SUFFIX "-dbg")
    set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
    set(CMAKE_RPM_DEBUGINFO_PACKAGE ON)
    set(CMAKE_TGZ_DEBUGINFO_PACKAGE ON)
else ()
    set(PACKAGE_NAME_SUFFIX "")
endif ()
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}${PACKAGE_NAME_SUFFIX}")
set(CPACK_DEFAULT_PACKAGE_DESCRIPTION_SUMMARY "waypp (Wayland Plus Plus library) - ${CMAKE_BUILD_TYPE}")

# Debian settings
set(CPACK_DEB_COMPONENT_INSTALL YES)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Joel Winarske <joel.winarske@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# RPM settings
set(CPACK_RPM_COMPONENT_INSTALL YES)
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_VENDOR "${CPACK_PACKAGE_VENDOR}")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_DEFAULT_PACKAGE_DESCRIPTION_SUMMARY}")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

# TGZ settings
set(CPACK_TGZ_FILE_NAME TGZ-DEFAULT)

include(CPack)