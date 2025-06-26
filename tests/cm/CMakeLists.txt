#
# Copyright (C) 2025 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aoscmcpp_test)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES identifierpool/identifierpool_test.cpp)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} aoscm aoscommon aostestcore aostestutils GTest::gmock_main)
target_include_directories(${TARGET} PRIVATE .)

gtest_discover_tests(${TARGET})
