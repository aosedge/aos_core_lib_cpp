#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aosiamcpp_test)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES
    iam_test.cpp
    certmodules/certmodules_test.cpp
    stubs/storagestub.cpp
    identmodules/fileidentifier_test.cpp
    nodeinfoprovider/nodeinfoprovider_test.cpp
    permhandler/permhandler_test.cpp
    provisionmanager/provisionmanager_test.cpp
    nodemanager/nodemanager_test.cpp
)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} aosiamcpp aoscommoncpp testutils GTest::gmock_main)
target_include_directories(${TARGET} PRIVATE ../include stubs)

gtest_discover_tests(${TARGET})
