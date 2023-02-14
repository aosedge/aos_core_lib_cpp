#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aosiamcpp)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES certhandler/certhandler.cpp)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(${TARGET} STATIC ${SOURCES})

# ######################################################################################################################
# Install
# ######################################################################################################################

set_target_properties(${TARGET} PROPERTIES PUBLIC_HEADER "certhandler/certhandler.hpp")

install(
    TARGETS ${TARGET}
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/aoscore/iam
)

# ######################################################################################################################
# Test
# ######################################################################################################################

if(WITH_TEST)
    set(TEST_SOURCES certhandler/certhandler_test.cpp)

    add_executable(${TARGET}_test ${TEST_SOURCES})
    target_link_libraries(${TARGET}_test GTest::gtest_main ${TARGET})

    gtest_discover_tests(${TARGET}_test)
endif()
