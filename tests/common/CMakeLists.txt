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

set(SOURCES src/stringer_test.cpp src/error_test.cpp src/log_test.cpp src/thread_test.cpp)

# ######################################################################################################################
# Test
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} GTest::gtest_main)

gtest_discover_tests(${TARGET})
