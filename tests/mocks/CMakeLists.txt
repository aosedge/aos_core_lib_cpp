#
# Copyright (C) 2025 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET buildmocks)

# ######################################################################################################################
# Generate test file
# ######################################################################################################################

file(GLOB_RECURSE HEADER_FILES "../include/mocks/*.hpp")

set(GENERATED_TEST_FILE ${CMAKE_BINARY_DIR}/generated_mocks.cpp)

file(WRITE ${GENERATED_TEST_FILE} "// Auto-generated file to test mock headers are compilable\n\n")

# Loop through each header and include it
foreach(HEADER ${HEADER_FILES})
    get_filename_component(HEADER_NAME ${HEADER} NAME)
    file(APPEND ${GENERATED_TEST_FILE} "#include \"mocks/${HEADER_NAME}\"\n")
endforeach()

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(${TARGET} OBJECT ${GENERATED_TEST_FILE})
target_link_libraries(${TARGET} PUBLIC aostestcore GTest::gmock_main)
