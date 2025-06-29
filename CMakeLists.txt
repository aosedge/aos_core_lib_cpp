#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.19)

# Check compiler using static library instead of applicion
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project("aos_core_lib_cpp")

# ######################################################################################################################
# Options
# ######################################################################################################################

option(WITH_TEST "build with test" OFF)
option(WITH_COVERAGE "build with coverage" OFF)
option(WITH_DOC "build with documenation" OFF)

message(STATUS)
message(STATUS "${CMAKE_PROJECT_NAME} configuration:")
message(STATUS "CMAKE_BUILD_TYPE              = ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX          = ${CMAKE_INSTALL_PREFIX}")
message(STATUS)
message(STATUS "WITH_TEST                     = ${WITH_TEST}")
message(STATUS "WITH_COVERAGE                 = ${WITH_COVERAGE}")
message(STATUS "WITH_DOC                      = ${WITH_DOC}")
message(STATUS)

# ######################################################################################################################
# Config
# ######################################################################################################################

set(AOS_CORE_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(AOS_STACK_USAGE 4096)

# ######################################################################################################################
# Compiler flags
# ######################################################################################################################

set(common_flags "-fPIC -Wall -Werror -Wextra -Wpedantic -Wno-format-truncation -Wno-stringop-truncation")

set(CMAKE_CXX_FLAGS "${common_flags} ${CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${common_flags} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_STANDARD 17)

if(WITH_TEST)
    add_definitions(-include ${AOS_CORE_LIB_DIR}/include/aos/test/aoscoretestconfig.hpp)
endif()

# ######################################################################################################################
# Dependencies
# ######################################################################################################################

set(CMAKE_MODULE_PATH ${AOS_CORE_LIB_DIR}/cmake)

if(WITH_TEST)
    find_package(GTest REQUIRED)

    include(GoogleTest)

    enable_testing()
endif()

if(WITH_COVERAGE)
    include(CodeCoverage)

    append_coverage_compiler_flags()

    set(COVERAGE_EXCLUDES "build/*" "tests/*" "/usr/*")
    set(GCOVR_ADDITIONAL_ARGS --gcov-ignore-parse-errors=negative_hits.warn_once_per_file)

    setup_target_for_coverage_lcov(
        NAME
        coverage
        SONARQUBE
        EXECUTABLE
        ctest
        LCOV_ARGS
        --ignore-errors
        mismatch,negative
    )
endif()

# ######################################################################################################################
# Includes
# ######################################################################################################################

include_directories(include)

# ######################################################################################################################
# Targets
# ######################################################################################################################

add_subdirectory(src/common)
add_subdirectory(src/sm)
add_subdirectory(src/iam)

if(WITH_TEST)
    add_subdirectory(tests)
endif()

# ######################################################################################################################
# Doc
# ######################################################################################################################

if(WITH_DOC)
    find_package(Doxygen)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doxygen.cfg ${CMAKE_CURRENT_BINARY_DIR}/doxygen.cfg @ONLY)

    add_custom_target(
        doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doxygen.cfg
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()
