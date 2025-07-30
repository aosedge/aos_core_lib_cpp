#
# Copyright (C) 2025 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

# ######################################################################################################################
# Target name
# ######################################################################################################################

set(TARGET_NAME version)

# ######################################################################################################################
# Dependencies
# ######################################################################################################################

find_package(Git)

if(NOT GIT_FOUND)
    set(GIT_EXECUTABLE git)
endif()

set(INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR}/include)

make_directory(${INCLUDE_PATH}/core/common/version)

execute_process(
    OUTPUT_FILE ${INCLUDE_PATH}/core/common/version/version.hpp
    COMMAND
        ${CMAKE_COMMAND} -D GIT_EXECUTABLE=${GIT_EXECUTABLE} -D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in
        -D OUTPUT_FILE=${INCLUDE_PATH}/core/common/version/version.hpp -D GIT_SOURCE_DIR=${PROJECT_SOURCE_DIR} -P
        ${VERSION_CMAKE_PATH}/GenerateVersion.cmake
)

# ######################################################################################################################
# Headers
# ######################################################################################################################

set(HEADERS ${INCLUDE_PATH}/core/common/version/version.hpp)

# ######################################################################################################################
# Includes
# ######################################################################################################################

set(INCLUDES ${INCLUDE_PATH})

# ######################################################################################################################
# Target
# ######################################################################################################################

set(HEADERS_BASE_DIRS ${INCLUDE_PATH})

add_module(TARGET_NAME ${TARGET_NAME} HEADERS ${HEADERS} INCLUDES ${INCLUDES})
