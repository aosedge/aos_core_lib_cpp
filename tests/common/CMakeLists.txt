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
    src/tools/enum_test.cpp
    src/tools/error_test.cpp
    src/tools/log_test.cpp
    src/tools/thread_test.cpp
    src/tools/buffer_test.cpp
    src/tools/array_test.cpp
    src/tools/ringbuffer_test.cpp
    src/tools/string_test.cpp
    src/tools/timer_test.cpp
)

# ######################################################################################################################
# Test
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} GTest::gtest_main)

gtest_discover_tests(${TARGET})
