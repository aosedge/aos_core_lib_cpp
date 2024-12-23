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
    certmodules/certmodules_test.cpp
    certprovider/certprovider_test.cpp
    identmodules/fileidentifier_test.cpp
    nodeinfoprovider/nodeinfoprovider_test.cpp
    nodemanager/nodemanager_test.cpp
    permhandler/permhandler_test.cpp
    provisionmanager/provisionmanager_test.cpp
    stubs/certhandlerstub.cpp
    iam_test.cpp
)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} aosiam aoscommon aostestutils GTest::gmock_main)
target_include_directories(${TARGET} PRIVATE .)

gtest_discover_tests(${TARGET})
