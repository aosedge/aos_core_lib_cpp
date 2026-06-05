#
# Copyright (C) 2024 Renesas Electronics Corporation.
# Copyright (C) 2024 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

# ######################################################################################################################
# Target name
# ######################################################################################################################

set(TARGET_NAME crypto)

# ######################################################################################################################
# Setup SoftHSM environment
# ######################################################################################################################

include(cmake/softhsm.cmake)
include(cmake/gencertificates.cmake)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES softhsmenv.cpp)

if(WITH_MBEDTLS)
    list(APPEND SOURCES providers/mbedtlsfactory.cpp)
endif()

if(WITH_OPENSSL)
    list(APPEND SOURCES providers/opensslfactory.cpp)
endif()

# ######################################################################################################################
# Headers
# ######################################################################################################################

set(HEADERS softhsmenv.hpp)

if(WITH_MBEDTLS)
    list(APPEND HEADERS providers/mbedtlsfactory.hpp)
endif()

if(WITH_OPENSSL)
    list(APPEND HEADERS providers/opensslfactory.hpp)
endif()

# ######################################################################################################################
# Libraries
# ######################################################################################################################

set(LIBRARIES aos::core::common::crypto aos::core::common::pkcs11)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_module(
    TARGET_NAME
    ${TARGET_NAME}
    LOG_MODULE
    SOURCES
    ${SOURCES}
    HEADERS
    ${HEADERS}
    LIBRARIES
    ${LIBRARIES}
)

# ######################################################################################################################
# Set SOFTHSM test environment
# ######################################################################################################################

createsofthsmtestenv(${TARGET} "${CMAKE_CURRENT_BINARY_DIR}/certificates")
gencertificates(${TARGET} "${CMAKE_CURRENT_BINARY_DIR}/softhsm")

# ######################################################################################################################
# Install
# ######################################################################################################################

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gencertificates.cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/softhsm.cmake
              ${CMAKE_CURRENT_SOURCE_DIR}/cmake/softhsm2.conf
        DESTINATION usr/share/cmake/aos/core/common/tests/crypto/cmake
)
