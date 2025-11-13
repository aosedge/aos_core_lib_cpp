#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME sm)

# ######################################################################################################################
# Headers
# ######################################################################################################################

set(HEADERS config.hpp)

# ######################################################################################################################
# Install targets
# ######################################################################################################################

set(INSTALL_HEADERS aos_core_sm_launcher aos_core_sm_logprovider aos_core_sm_runner)

set(INSTALL_LIBRARIES)

if(WITH_TEST)
    list(APPEND INSTALL_HEADERS aos_core_sm_tests_mocks aos_core_sm_tests_stubs)
endif()

# ######################################################################################################################
# Target
# ######################################################################################################################

add_module(TARGET_NAME ${TARGET_NAME} HEADERS ${HEADERS} OBJECTS ${INSTALL_LIBRARIES})

# ######################################################################################################################
# Target properties
# ######################################################################################################################

set(TARGET_NAMESPACE ${TARGET_NAMESPACE}::sm)
set(TARGET_PREFIX ${TARGET_PREFIX}_sm)

# ######################################################################################################################
# Subdirectories
# ######################################################################################################################

add_subdirectory(launcher)
add_subdirectory(logprovider)
add_subdirectory(networkmanager)
add_subdirectory(runner)
add_subdirectory(smclient)

if(WITH_TEST_TOOLS)
    add_subdirectory(tests)
endif()

# ######################################################################################################################
# Install
# ######################################################################################################################

install(TARGETS ${TARGET} ARCHIVE FILE_SET HEADERS DESTINATION include/aos)

install(
    TARGETS ${INSTALL_HEADERS} ${INSTALL_LIBRARIES} FILE_SET HEADERS
    DESTINATION include/aos
    ARCHIVE EXCLUDE_FROM_ALL
)
