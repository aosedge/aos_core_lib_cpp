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
    aos_core_cm_fileserver
    aos_core_cm_iamclient
    aos_core_cm_launcher
    aos_core_cm_networkmanager
    aos_core_cm_nodeinfoprovider
    aos_core_cm_smcontroller
    aos_core_cm_storagestate
    aos_core_cm_unitconfig
)

set(INSTALL_LIBRARIES aos_core_cm_alerts aos_core_cm_launcher aos_core_cm_monitoring aos_core_cm_storagestate
                      aos_core_cm_updatemanager
)

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

add_subdirectory(alerts)
add_subdirectory(communication)
add_subdirectory(fileserver)
add_subdirectory(iamclient)
add_subdirectory(launcher)
add_subdirectory(monitoring)
add_subdirectory(networkmanager)
add_subdirectory(nodeinfoprovider)
add_subdirectory(smcontroller)
add_subdirectory(storagestate)
add_subdirectory(unitconfig)
add_subdirectory(updatemanager)

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
