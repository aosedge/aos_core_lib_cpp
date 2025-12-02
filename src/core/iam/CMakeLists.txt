#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME iam)

# ######################################################################################################################
# Headers
# ######################################################################################################################

set(HEADERS config.hpp)

# ######################################################################################################################
# Install targets
# ######################################################################################################################

set(INSTALL_HEADERS aos_core_iam_identhandler aos_core_iam_nodeinfoprovider)

set(INSTALL_LIBRARIES aos_core_iam_certhandler aos_core_iam_certmodules_pkcs11 aos_core_iam_identmodules_fileidentifier
                      aos_core_iam_nodemanager aos_core_iam_permhandler aos_core_iam_provisionmanager
)

if(WITH_TEST)
    list(APPEND INSTALL_HEADERS aos_core_iam_tests_mocks)
    list(APPEND INSTALL_LIBRARIES aos_core_iam_tests_stubs)
endif()

# ######################################################################################################################
# Target
# ######################################################################################################################

add_module(TARGET_NAME ${TARGET_NAME} HEADERS ${HEADERS} OBJECTS ${INSTALL_LIBRARIES})

# ######################################################################################################################
# Target properties
# ######################################################################################################################

set(TARGET_NAMESPACE ${TARGET_NAMESPACE}::iam)
set(TARGET_PREFIX ${TARGET_PREFIX}_iam)

# ######################################################################################################################
# Subdirectories
# ######################################################################################################################

add_subdirectory(certhandler)
add_subdirectory(identhandler)
add_subdirectory(nodeinfoprovider)
add_subdirectory(nodemanager)
add_subdirectory(permhandler)
add_subdirectory(provisionmanager)

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
