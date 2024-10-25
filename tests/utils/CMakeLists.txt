#
# Copyright (C) 2024 Renesas Electronics Corporation.
# Copyright (C) 2024 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET testutils)

# ######################################################################################################################
# Setup softhsm environment
# ######################################################################################################################

include(cmake/softhsm.cmake)
include(cmake/gencertificates.cmake)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES src/softhsmenv.cpp)

# ######################################################################################################################
# Set SOFTHSM test environment
# ######################################################################################################################

add_library(${TARGET} ${SOURCES})

createsofthsmtestenv(${TARGET} "${CMAKE_CURRENT_BINARY_DIR}/certificates")
gencertificates(${TARGET} "${CMAKE_CURRENT_BINARY_DIR}/softhsm")

target_include_directories(${TARGET} PUBLIC ${AOS_CORE_LIB_DIR}/include)

# ######################################################################################################################
# Install
# ######################################################################################################################

install(TARGETS ${TARGET} ARCHIVE DESTINATION lib)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/aos/test
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h"
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gencertificates.cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/softhsm.cmake
              ${CMAKE_CURRENT_SOURCE_DIR}/cmake/softhsm2.conf DESTINATION lib/cmake/test/utils
)
