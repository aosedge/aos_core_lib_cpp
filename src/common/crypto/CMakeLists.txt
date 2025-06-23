#
# Copyright (C) 2025 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aoscrypto)

option(WITH_MBEDTLS "build with MbedTLS" ON)
option(WITH_OPENSSL "build with OpenSSL" OFF)

message(STATUS "WITH_MBEDTLS                  = ${WITH_MBEDTLS}")
message(STATUS "WITH_OPENSSL                  = ${WITH_OPENSSL}")
message(STATUS)

# ######################################################################################################################
# Includes
# ######################################################################################################################

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/drivers)

# ######################################################################################################################
# Fetch and setup MbedTLS
# ######################################################################################################################

if(WITH_MBEDTLS)
    include(FetchContent)

    set(FETCHCONTENT_QUIET FALSE)

    FetchContent_Declare(
        mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG v3.6.1
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        PATCH_COMMAND
            cp ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/drivers/psa_crypto_driver_wrappers_no_static.c
            ${CMAKE_BINARY_DIR}/_deps/mbedtls-src/library/psa_crypto_driver_wrappers_no_static.c && cp
            ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/drivers/psa_crypto_driver_wrappers.h
            ${CMAKE_BINARY_DIR}/_deps/mbedtls-src/library/psa_crypto_driver_wrappers.h && cp
            ${AOS_CORE_LIB_DIR}/include/aos/common/crypto/mbedtls/mbedtls_config.h
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

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(${TARGET} STATIC crypto.cpp cryptoutils.cpp)

if(WITH_MBEDTLS)
    target_sources(${TARGET} PRIVATE mbedtls/cryptoprovider.cpp mbedtls/driverwrapper.cpp)
    target_link_libraries(${TARGET} PUBLIC mbedcrypto mbedx509 mbedtls)
    target_compile_definitions(${TARGET} PUBLIC WITH_MBEDTLS)
endif()

if(WITH_OPENSSL)
    find_package(OpenSSL REQUIRED)

    target_sources(${TARGET} PRIVATE openssl/cryptoprovider.cpp openssl/opensslprovider.cpp)
    target_link_libraries(${TARGET} PUBLIC OpenSSL::Crypto)
    # Suppress unused-function warning for SEQ_OID macro
    target_compile_options(${TARGET} PRIVATE -Wno-unused-function)
    target_compile_definitions(${TARGET} PUBLIC WITH_OPENSSL)
endif()

target_compile_definitions(${TARGET} PRIVATE LOG_MODULE="crypto")
target_compile_options(${TARGET} PRIVATE -Wstack-usage=${AOS_STACK_USAGE})
target_include_directories(${TARGET} PRIVATE ${AOS_CORE_LIB_DIR}/include)
