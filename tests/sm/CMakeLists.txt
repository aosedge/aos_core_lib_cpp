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

set(SOURCES
    # launcher
    ${CMAKE_SOURCE_DIR}/src/sm/launcher/launcher.cpp
    ${CMAKE_SOURCE_DIR}/src/sm/launcher/instance.cpp
    ${CMAKE_SOURCE_DIR}/src/sm/launcher/service.cpp
    launcher/launcher_test.cpp
    # servicemanager
    ${CMAKE_SOURCE_DIR}/src/sm/servicemanager/servicemanager.cpp
    servicemanager/servicemanager_test.cpp
)

# ######################################################################################################################
# Includes
# ######################################################################################################################

include_directories(${CMAKE_SOURCE_DIR})

# ######################################################################################################################
# Definitions
# ######################################################################################################################

add_definitions(
    -DAOS_CONFIG_LAUNCHER_RUNTIME_DIR="/tmp/aos/runtime" -DAOS_CONFIG_SERVICEMANAGER_SERVICES_DIR="/tmp/aos/services"
)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} GTest::gtest_main)

gtest_discover_tests(${TARGET})
