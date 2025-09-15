#
# Copyright (C) 2025 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

# ######################################################################################################################
# Target name
# ######################################################################################################################

set(TARGET_NAME crypto)

# ######################################################################################################################
# Options
# ######################################################################################################################

option(WITH_MBEDTLS "build with MbedTLS" ON)
option(WITH_OPENSSL "build with OpenSSL" OFF)

message(STATUS "WITH_MBEDTLS                  = ${WITH_MBEDTLS}")
message(STATUS "WITH_OPENSSL                  = ${WITH_OPENSSL}")
message(STATUS)

# ######################################################################################################################
# Dependencies
# ######################################################################################################################

if(WITH_MBEDTLS)
    include(FetchContent)

    set(FETCHCONTENT_QUIET FALSE)

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/drivers)

    FetchContent_Declare(
        mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG v3.6.1
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
        PATCH_COMMAND
            cp ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/drivers/psa_crypto_driver_wrappers_no_static.c
            ${CMAKE_BINARY_DIR}/_deps/mbedtls-src/library/psa_crypto_driver_wrappers_no_static.c && cp
            ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/drivers/psa_crypto_driver_wrappers.h
            ${CMAKE_BINARY_DIR}/_deps/mbedtls-src/library/psa_crypto_driver_wrappers.h && cp
            ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/mbedtls_config.h
            ${CMAKE_BINARY_DIR}/_deps/mbedtls-src/include/mbedtls/mbedtls_config.h
    )

    set(ENABLE_TESTING
        OFF
        CACHE BOOL "" FORCE
    )
    set(ENABLE_PROGRAMS
        OFF
        CACHE BOOL "" FORCE
    )

    FetchContent_MakeAvailable(mbedtls)

    get_target_property(MBEDTLS_INCLDUES mbedtls INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(mbedtls SYSTEM INTERFACE ${MBEDTLS_INCLDUES})
endif()

if(WITH_OPENSSL)
    find_package(OpenSSL REQUIRED)
endif()

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES crypto.cpp cryptohelper.cpp cryptoutils.cpp)

if(WITH_MBEDTLS)
    list(APPEND SOURCES mbedtls/cryptoprovider.cpp mbedtls/driverwrapper.cpp)
endif()

if(WITH_OPENSSL)
    list(APPEND SOURCES openssl/cryptoprovider.cpp openssl/opensslprovider.cpp)
endif()

# ######################################################################################################################
# Headers
# ######################################################################################################################

set(HEADERS crypto.hpp cryptoprovider.hpp cryptoutils.hpp)

if(WITH_MBEDTLS)
    list(APPEND HEADERS mbedtls/cryptoprovider.hpp mbedtls/driverwrapper.hpp)
endif()

if(WITH_OPENSSL)
    list(APPEND HEADERS openssl/cryptoprovider.hpp openssl/opensslprovider.hpp)
endif()

# ######################################################################################################################
# Defines
# ######################################################################################################################

if(WITH_MBEDTLS)
    list(APPEND DEFINES WITH_MBEDTLS)
endif()

if(WITH_OPENSSL)
    list(APPEND DEFINES WITH_OPENSSL)
endif()

# ######################################################################################################################
# Compile options
# ######################################################################################################################

if(WITH_OPENSSL)
    set(COMPILE_OPTIONS -Wno-unused-function)
endif()

# ######################################################################################################################
# Libraries
# ######################################################################################################################

set(LIBRARIES aos::core::common::tools)

if(WITH_MBEDTLS)
    list(APPEND LIBRARIES mbedcrypto mbedx509 mbedtls)
endif()

if(WITH_OPENSSL)
    list(APPEND LIBRARIES OpenSSL::Crypto)
endif()

# ######################################################################################################################
# Target
# ######################################################################################################################

add_module(
    TARGET_NAME
    ${TARGET_NAME}
    LOG_MODULE
    STACK_USAGE
    ${AOS_STACK_USAGE}
    SOURCES
    ${SOURCES}
    HEADERS
    ${HEADERS}
    DEFINES
    ${DEFINES}
    COMPILE_OPTIONS
    ${COMPILE_OPTIONS}
    LIBRARIES
    ${LIBRARIES}
)

# ######################################################################################################################
# Tests
# ######################################################################################################################

if(WITH_TEST)
    add_subdirectory(tests)
endif()
