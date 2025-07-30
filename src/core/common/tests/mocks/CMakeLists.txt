#
# Copyright (C) 2025 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

# ######################################################################################################################
# Target name
# ######################################################################################################################

set(TARGET_NAME mocks)

# ######################################################################################################################
# Headers
# ######################################################################################################################

set(HEADERS
    alertsmock.hpp
    certhandlermock.hpp
    certloadermock.hpp
    certprovidermock.hpp
    cnimock.hpp
    communicationmock.hpp
    connectionprovidermock.hpp
    cryptomock.hpp
    downloadermock.hpp
    fsmock.hpp
    identhandlermock.hpp
    launchermock.hpp
    logprovidermock.hpp
    monitoringmock.hpp
    networkmanagermock.hpp
    nodeinfoprovidermock.hpp
    nodemanagermock.hpp
    ocispecmock.hpp
    permhandlermock.hpp
    provisionmanagermock.hpp
    resourcemanagermock.hpp
    runnermock.hpp
    spaceallocatormock.hpp
)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_module(TARGET_NAME ${TARGET_NAME} HEADERS ${HEADERS})
