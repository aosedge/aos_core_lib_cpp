#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aosiamcpp)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES
    certhandler/certhandler.cpp
    certmodules/certmodule.cpp
    certmodules/pkcs11/pkcs11.cpp
    identmodules/fileidentifier.cpp
    nodeinfoprovider/nodeinfoprovider.cpp
    nodemanager/nodemanager.cpp
    permhandler/permhandler.cpp
    provisionmanager/provisionmanager.cpp
)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(${TARGET} STATIC ${SOURCES})

# ######################################################################################################################
# Install
# ######################################################################################################################

install(TARGETS ${TARGET} ARCHIVE DESTINATION lib)

install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/include/aos/iam
    DESTINATION include/aos
    FILES_MATCHING
    PATTERN "*.hpp"
)
