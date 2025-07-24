#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aoscmcpp_test)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES
    # launcher
    ${PROJECT_SOURCE_DIR}/src/cm/launcher/instancemanager.cpp ${PROJECT_SOURCE_DIR}/src/cm/launcher/launcher.cpp
    ${PROJECT_SOURCE_DIR}/src/cm/launcher/nodehandler.cpp ${PROJECT_SOURCE_DIR}/src/cm/launcher/servicebalancer.cpp
    launcher/launcher_test.cpp
)

# ######################################################################################################################
# Includes
# ######################################################################################################################

include_directories(${PROJECT_SOURCE_DIR}/tests/include)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} aoscommon aostestcore aostestutils GTest::gmock_main)
target_include_directories(${TARGET} PRIVATE .)

gtest_discover_tests(${TARGET})
