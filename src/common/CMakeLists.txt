#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aoscommoncpp)

# ######################################################################################################################
# Versioning
# ######################################################################################################################

find_package(Git)

if(NOT GIT_FOUND)
    set(GIT_EXECUTABLE git)
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.hpp
    COMMAND
        ${CMAKE_COMMAND} -D GIT_EXECUTABLE=${GIT_EXECUTABLE} -D
        INPUT_FILE=${CMAKE_SOURCE_DIR}/src/common/version.hpp.in -D OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/version.hpp
        -D GIT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_MODULE_PATH}/GenerateVersion.cmake
)

# ######################################################################################################################
# Target
# ######################################################################################################################
add_library(${TARGET} STATIC resourcemonitor/resourcemonitor.cpp)

target_sources(${TARGET} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/version.hpp)

# ######################################################################################################################
# Install
# ######################################################################################################################

install(TARGETS ${TARGET} ARCHIVE DESTINATION lib)

install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/include/aos/common
    DESTINATION include/aos
    FILES_MATCHING
    PATTERN "*.hpp"
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version.hpp DESTINATION include/aos/common)
