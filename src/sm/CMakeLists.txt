#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aossmcpp)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES launcher/launcher.cpp)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(${TARGET} STATIC ${SOURCES})

# ######################################################################################################################
# Install
# ######################################################################################################################

install(TARGETS ${TARGET} ARCHIVE DESTINATION lib)

install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/include/aos/sm
    DESTINATION include/aos
    FILES_MATCHING
    PATTERN "*.hpp"
)

# ######################################################################################################################
# Test
# ######################################################################################################################

if(WITH_TEST)
    set(TEST_SOURCES launcher/launcher_test.cpp)

    add_executable(${TARGET}_test ${TEST_SOURCES})
    target_link_libraries(${TARGET}_test GTest::gtest_main ${TARGET})

    gtest_discover_tests(${TARGET}_test)
endif()
