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
    ${PROJECT_SOURCE_DIR}/src/sm/launcher/launcher.cpp
    ${PROJECT_SOURCE_DIR}/src/sm/launcher/instance.cpp
    ${PROJECT_SOURCE_DIR}/src/sm/launcher/service.cpp
    launcher/launcher_test.cpp
    # layermanager
    ${PROJECT_SOURCE_DIR}/src/sm/layermanager/layermanager.cpp
    layermanager/layermanager_test.cpp
    # resourcemanager
    ${PROJECT_SOURCE_DIR}/src/sm/resourcemanager/resourcemanager.cpp
    resourcemanager/resourcemanager_test.cpp
    # servicemanager
    ${PROJECT_SOURCE_DIR}/src/sm/servicemanager/servicemanager.cpp
    servicemanager/servicemanager_test.cpp
    #networkmanager
    ${PROJECT_SOURCE_DIR}/src/sm/networkmanager/networkmanager.cpp
    networkmanager/networkmanager_test.cpp
)

# ######################################################################################################################
# Includes
# ######################################################################################################################

include_directories(${PROJECT_SOURCE_DIR}/tests/include)

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
target_link_libraries(${TARGET} aoscommon GTest::gmock_main testutils)
target_include_directories(${TARGET} PRIVATE .)

gtest_discover_tests(${TARGET})
