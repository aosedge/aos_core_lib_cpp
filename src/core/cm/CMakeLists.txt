#
# Copyright (C) 2025 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME cm)

# ######################################################################################################################
# Headers
# ######################################################################################################################

set(HEADERS config.hpp)

# ######################################################################################################################
# Install targets
# ######################################################################################################################

set(INSTALL_HEADERS
    aos_core_cm_communication
    aos_core_cm_iamclient
    aos_core_cm_imagemanager
    aos_core_cm_launcher
    aos_core_cm_networkmanager
    aos_core_cm_nodeinfoprovider
    aos_core_cm_resourcemanager
    aos_core_cm_smcontroller
    aos_core_cm_storagestate
)

set(INSTALL_LIBRARIES)

if(WITH_TEST)
    list(APPEND INSTALL_HEADERS aos_core_cm_tests_mocks)
endif()

# ######################################################################################################################
# Target
# ######################################################################################################################

add_module(TARGET_NAME ${TARGET_NAME} HEADERS ${HEADERS} OBJECTS ${INSTALL_LIBRARIES})

# ######################################################################################################################
# Target properties
# ######################################################################################################################

set(TARGET_NAMESPACE ${TARGET_NAMESPACE}::cm)
set(TARGET_PREFIX ${TARGET_PREFIX}_cm)

# ######################################################################################################################
# Subdirectories
# ######################################################################################################################

add_subdirectory(communication)
add_subdirectory(iamclient)
add_subdirectory(imagemanager)
add_subdirectory(launcher)
add_subdirectory(networkmanager)
add_subdirectory(nodeinfoprovider)
add_subdirectory(resourcemanager)
add_subdirectory(smcontroller)
add_subdirectory(storagestate)

if(WITH_TEST)
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
