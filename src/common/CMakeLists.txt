#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aoscommoncpp)

option(WITH_MBEDTLS "build with mbedTLS" ON)

message(STATUS "WITH_MBEDTLS                  = ${WITH_MBEDTLS}")
message(STATUS)

# ######################################################################################################################
# Versioning
# ######################################################################################################################

find_package(Git)

if(NOT GIT_FOUND)
    set(GIT_EXECUTABLE git)
endif()

execute_process(
    OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.hpp
    COMMAND
        ${CMAKE_COMMAND} -D GIT_EXECUTABLE=${GIT_EXECUTABLE} -D
        INPUT_FILE=${CMAKE_SOURCE_DIR}/src/common/version.hpp.in -D OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/version.hpp
        -D GIT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_MODULE_PATH}/GenerateVersion.cmake
)

# ######################################################################################################################
# Includes
# ######################################################################################################################

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ######################################################################################################################
# Fetch and setup MbedTLS
# ######################################################################################################################

if(WITH_MBEDTLS)
    include(FetchContent)

    set(FETCHCONTENT_QUIET FALSE)

    FetchContent_Declare(
        mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG v3.5.0
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        PATCH_COMMAND
            cp ${CMAKE_CURRENT_SOURCE_DIR}/crypto/mbedtls/drivers/psa_crypto_driver_wrappers_no_static.c
            ${CMAKE_BINARY_DIR}/_deps/mbedtls-src/library/psa_crypto_driver_wrappers_no_static.c && cp
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/mbedtls/drivers/psa_crypto_driver_wrappers.h
            ${CMAKE_BINARY_DIR}/_deps/mbedtls-src/library/psa_crypto_driver_wrappers.h && cp
            ${CMAKE_SOURCE_DIR}/include/aos/common/crypto/mbedtls/mbedtls_config.h
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
endif()

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(
    ${TARGET} STATIC
    crypto/mbedtls/cryptoprovider.cpp
    crypto/mbedtls/driverwrapper.cpp
    cryptoutils.cpp
    monitoring/monitoring.cpp
    pkcs11/pkcs11.cpp
    pkcs11/privatekey.cpp
    uuid.cpp
)

target_sources(${TARGET} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/version.hpp)

if(WITH_MBEDTLS)
    target_link_libraries(${TARGET} PUBLIC mbedcrypto mbedx509 mbedtls)
endif()

# ######################################################################################################################
# Install
# ######################################################################################################################

install(TARGETS ${TARGET} ARCHIVE DESTINATION lib)

install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/include/aos/common
    DESTINATION include/aos
    FILES_MATCHING
    PATTERN "*.hpp"
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version.hpp DESTINATION include/aos/common)
