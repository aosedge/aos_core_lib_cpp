#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aossmcpp_test)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES ${CMAKE_SOURCE_DIR}/src/sm/launcher/launcher.cpp ${CMAKE_SOURCE_DIR}/src/sm/launcher/instance.cpp
            ${CMAKE_SOURCE_DIR}/src/sm/launcher/service.cpp launcher/launcher_test.cpp
)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} GTest::gtest_main)

gtest_discover_tests(${TARGET})
