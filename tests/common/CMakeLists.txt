#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aoscommoncpp_test)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES
    src/tools/allocator_test.cpp
    src/tools/array_test.cpp
    src/tools/buffer_test.cpp
    src/tools/enum_test.cpp
    src/tools/error_test.cpp
    src/tools/function_test.cpp
    src/tools/log_test.cpp
    src/tools/memory_test.cpp
    src/tools/optional_test.cpp
    src/tools/queue_test.cpp
    src/tools/thread_test.cpp
    src/tools/time_test.cpp
    src/pkcs11/pkcs11_test.cpp
    src/monitoring_test.cpp
    src/types_test.cpp
    src/uuid_test.cpp
    ${CMAKE_SOURCE_DIR}/src/common/monitoring/monitoring.cpp
    src/crypto_test.cpp
)

# ######################################################################################################################
# Definitions
# ######################################################################################################################

add_definitions(-DAOS_CONFIG_MONITORING_SEND_PERIOD_SEC=2 -DAOS_CONFIG_MONITORING_POLL_PERIOD_SEC=1)

# ######################################################################################################################
# Setup SoftHSM2
# ######################################################################################################################

find_library(
    SOFTHSM2_LIB
    NAMES softhsm2
    PATH_SUFFIXES softhsm
)

if(NOT SOFTHSM2_LIB)
    message(FATAL_ERROR "softhsm2 library not found")
endif()

set(SOFTHSM_BASE ${CMAKE_CURRENT_BINARY_DIR}/softhsm)

file(MAKE_DIRECTORY ${SOFTHSM_BASE}/tokens/)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/pkcs11/softhsm2.conf ${SOFTHSM_BASE}/softhsm2.conf COPYONLY)
file(APPEND ${SOFTHSM_BASE}/softhsm2.conf "directories.tokendir = ${SOFTHSM_BASE}/tokens/\n")

add_definitions(-DSOFTHSM2_CONF="${SOFTHSM_BASE}/softhsm2.conf" -DSOFTHSM2_LIB="${SOFTHSM2_LIB}")

# ######################################################################################################################
# Setup PKCS11 test certificates
# ######################################################################################################################

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pkcs11/pkcs11cert.der ${CMAKE_CURRENT_BINARY_DIR}/pkcs11cert.der COPYONLY
)
add_definitions(-DPKCS11_TESTCERT="${CMAKE_CURRENT_BINARY_DIR}/pkcs11cert.der")

# ######################################################################################################################
# Test
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(
    ${TARGET}
    aoscommoncpp
    mbedcrypto
    mbedx509
    mbedtls
    aoscommoncpp
    GTest::gmock_main
)
target_include_directories(${TARGET} PUBLIC ../include)

gtest_discover_tests(${TARGET})
