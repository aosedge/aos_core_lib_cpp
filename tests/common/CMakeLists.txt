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
    ${CMAKE_SOURCE_DIR}/src/common/monitoring/monitoring.cpp
    src/crypto_test.cpp
    src/cryptoutils_test.cpp
    src/monitoring_test.cpp
    src/pkcs11_test.cpp
    src/tools/allocator_test.cpp
    src/tools/array_test.cpp
    src/tools/buffer_test.cpp
    src/tools/enum_test.cpp
    src/tools/error_test.cpp
    src/tools/function_test.cpp
    src/tools/log_test.cpp
    src/tools/map_test.cpp
    src/tools/memory_test.cpp
    src/tools/optional_test.cpp
    src/tools/queue_test.cpp
    src/tools/string_test.cpp
    src/tools/thread_test.cpp
    src/tools/time_test.cpp
    src/tools/uuid_test.cpp
    src/tools/variant_test.cpp
    src/types_test.cpp
)

# ######################################################################################################################
# Definitions
# ######################################################################################################################

add_definitions(-DAOS_CONFIG_MONITORING_SEND_PERIOD_SEC=2 -DAOS_CONFIG_MONITORING_POLL_PERIOD_SEC=1)

# ######################################################################################################################
# Test
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} aoscommoncpp testutils GTest::gmock_main)

gtest_discover_tests(${TARGET})
