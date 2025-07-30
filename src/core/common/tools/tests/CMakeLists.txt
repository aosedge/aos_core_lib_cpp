#
# Copyright (C) 2025 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME tools_test)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES
    allocator.cpp
    array.cpp
    buffer.cpp
    enum.cpp
    error.cpp
    fs.cpp
    function.cpp
    identifierpool.cpp
    list.cpp
    log.cpp
    map.cpp
    memory.cpp
    optional.cpp
    queue.cpp
    ringbuffer.cpp
    semver.cpp
    string.cpp
    thread.cpp
    time.cpp
    timer.cpp
    uuid.cpp
    variant.cpp
)

# ######################################################################################################################
# Libraries
# ######################################################################################################################

set(LIBRARIES aos::core::common::tools GTest::gmock_main)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_test(
    TARGET_NAME
    ${TARGET_NAME}
    LOG_MODULE
    SOURCES
    ${SOURCES}
    LIBRARIES
    ${LIBRARIES}
)
