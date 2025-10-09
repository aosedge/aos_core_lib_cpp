#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME common)

# ######################################################################################################################
# Headers
# ######################################################################################################################

set(HEADERS config.hpp)

# ######################################################################################################################
# Install targets
# ######################################################################################################################

set(INSTALL_HEADERS
    aos_core_common_alerts
    aos_core_common_connectionprovider
    aos_core_common_logprovider
    aos_core_common_ocispec
    aos_core_common_spaceallocator
    aos_core_common_types
    aos_core_common_version
)

set(INSTALL_LIBRARIES aos_core_common_crypto aos_core_common_monitoring aos_core_common_pkcs11 aos_core_common_tools)

if(WITH_TEST)
    list(APPEND INSTALL_HEADERS aos_core_common_tests_mocks aos_core_common_tests_stubs)
    list(APPEND INSTALL_LIBRARIES aos_core_common_tests_crypto aos_core_common_tests_utils)
endif()

# ######################################################################################################################
# Target
# ######################################################################################################################

add_module(TARGET_NAME ${TARGET_NAME} HEADERS ${HEADERS} OBJECTS ${INSTALL_LIBRARIES})

# ######################################################################################################################
# Target properties
# ######################################################################################################################

set(TARGET_NAMESPACE ${TARGET_NAMESPACE}::common)
set(TARGET_PREFIX ${TARGET_PREFIX}_common)

# ######################################################################################################################
# Subdirectories
# ######################################################################################################################

add_subdirectory(alerts)
add_subdirectory(connectionprovider)
add_subdirectory(crypto)
add_subdirectory(downloader)
add_subdirectory(logprovider)
add_subdirectory(monitoring)
add_subdirectory(ocispec)
add_subdirectory(pkcs11)
add_subdirectory(spaceallocator)
add_subdirectory(tools)
add_subdirectory(types)
add_subdirectory(version)

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
