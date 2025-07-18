#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aoscm)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES launcher/instancemanager.cpp launcher/launcher.cpp launcher/nodehandler.cpp launcher/servicebalancer.cpp)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(${TARGET} STATIC ${SOURCES})

target_compile_options(${TARGET} PRIVATE -Wstack-usage=${AOS_STACK_USAGE})
target_include_directories(${TARGET} PUBLIC ${AOS_CORE_LIB_DIR}/include)
target_link_libraries(${TARGET} PRIVATE aoscommon)

# ######################################################################################################################
# Install
# ######################################################################################################################

install(TARGETS ${TARGET} ARCHIVE DESTINATION lib)

install(
    DIRECTORY ${AOS_CORE_LIB_DIR}/include/aos/cm
    DESTINATION include/aos
    FILES_MATCHING
    PATTERN "*.hpp"
)
