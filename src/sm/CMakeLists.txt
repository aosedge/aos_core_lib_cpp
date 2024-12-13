#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aossm)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES launcher/launcher.cpp launcher/instance.cpp launcher/service.cpp layermanager/layermanager.cpp
            resourcemanager/resourcemanager.cpp servicemanager/servicemanager.cpp
)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(${TARGET} STATIC ${SOURCES})
target_link_libraries(${TARGET} PRIVATE aoscommon)

target_include_directories(${TARGET} PUBLIC ${AOS_CORE_LIB_DIR}/include)

# ######################################################################################################################
# Install
# ######################################################################################################################

install(TARGETS ${TARGET} ARCHIVE DESTINATION lib)

install(
    DIRECTORY ${AOS_CORE_LIB_DIR}/include/aos/sm
    DESTINATION include/aos
    FILES_MATCHING
    PATTERN "*.hpp"
)
