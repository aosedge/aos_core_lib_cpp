#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aoscommon_test)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES
    ${PROJECT_SOURCE_DIR}/src/common/monitoring/average.cpp
    ${PROJECT_SOURCE_DIR}/src/common/monitoring/alertprocessor.cpp
    ${PROJECT_SOURCE_DIR}/src/common/monitoring/resourcemonitor.cpp
    src/alertprocessor_test.cpp
    src/crypto/crypto_test.cpp
    src/monitoring_test.cpp
    src/ocispec_test.cpp
    src/tools/allocator_test.cpp
    src/tools/array_test.cpp
    src/tools/buffer_test.cpp
    src/tools/enum_test.cpp
    src/tools/error_test.cpp
    src/tools/fs_test.cpp
    src/tools/function_test.cpp
    src/tools/list_test.cpp
    src/tools/log_test.cpp
    src/tools/map_test.cpp
    src/tools/memory_test.cpp
    src/tools/optional_test.cpp
    src/tools/queue_test.cpp
    src/tools/semver_test.cpp
    src/tools/string_test.cpp
    src/tools/thread_test.cpp
    src/tools/time_test.cpp
    src/tools/timer_test.cpp
    src/tools/uuid_test.cpp
    src/tools/variant_test.cpp
    src/types_test.cpp
)

if(WITH_MBEDTLS OR WITH_OPENSSL)
    set(SOURCES ${SOURCES} src/crypto/cryptoprovider_test.cpp src/crypto/cryptoutils_test.cpp src/pkcs11_test.cpp)
endif()

# ######################################################################################################################
# Includes
# ######################################################################################################################

include_directories(${PROJECT_SOURCE_DIR}/tests/include)

# ######################################################################################################################
# Definitions
# ######################################################################################################################

add_definitions(-DAOS_CONFIG_MONITORING_POLL_PERIOD_SEC=1 -DAOS_CONFIG_MONITORING_AVERAGE_WINDOW_SEC=3)

# ######################################################################################################################
# Test
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} aostestutils GTest::gmock_main)

gtest_discover_tests(${TARGET})
