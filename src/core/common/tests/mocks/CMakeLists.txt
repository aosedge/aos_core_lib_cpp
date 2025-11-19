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
    certprovidermock.hpp
    cloudconnectionmock.hpp
    cryptomock.hpp
    downloadermock.hpp
    fileinfoprovidermock.hpp
    fsmock.hpp
    identprovidermock.hpp
    instancestatusprovidermock.hpp
    logprovidermock.hpp
    monitoringmock.hpp
    ocispecmock.hpp
    permhandlermock.hpp
    permprovidermock.hpp
    spaceallocatormock.hpp
)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_module(TARGET_NAME ${TARGET_NAME} HEADERS ${HEADERS})
