#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aoscommon)

option(WITH_MBEDTLS "build with mbedTLS" ON)

message(STATUS "WITH_MBEDTLS                  = ${WITH_MBEDTLS}")
message(STATUS)

# ######################################################################################################################
# Versioning
# ######################################################################################################################

set(CMAKE_MODULE_PATH ${AOS_CORE_LIB_DIR}/cmake)

find_package(Git)

if(NOT GIT_FOUND)
    set(GIT_EXECUTABLE git)
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/aos/common)

execute_process(
    OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/include/aos/common/version.hpp
    COMMAND
        ${CMAKE_COMMAND} -D GIT_EXECUTABLE=${GIT_EXECUTABLE} -D
        INPUT_FILE=${AOS_CORE_LIB_DIR}/src/common/version.hpp.in -D
        OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/include/aos/common/version.hpp -D GIT_SOURCE_DIR=${AOS_CORE_LIB_DIR} -P
        ${CMAKE_MODULE_PATH}/GenerateVersion.cmake
)

# ######################################################################################################################
# Includes
# ######################################################################################################################

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/crypto/mbedtls/drivers)

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
            cp ${CMAKE_CURRENT_SOURCE_DIR}/crypto/mbedtls/drivers/psa_crypto_driver_wrappers_no_static.c
            ${CMAKE_BINARY_DIR}/_deps/mbedtls-src/library/psa_crypto_driver_wrappers_no_static.c && cp
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/mbedtls/drivers/psa_crypto_driver_wrappers.h
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

add_library(
    ${TARGET} STATIC
    crypto/mbedtls/cryptoprovider.cpp
    crypto/mbedtls/driverwrapper.cpp
    crypto/crypto.cpp
    crypto/cryptoutils.cpp
    monitoring/average.cpp
    monitoring/alertprocessor.cpp
    monitoring/resourcemonitor.cpp
    pkcs11/pkcs11.cpp
    pkcs11/privatekey.cpp
    tools/semver.cpp
    tools/uuid.cpp
)

target_sources(${TARGET} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include/aos/common/version.hpp)

if(WITH_MBEDTLS)
    target_link_libraries(${TARGET} PUBLIC mbedcrypto mbedx509 mbedtls)
endif()

target_compile_options(${TARGET} PRIVATE -Wstack-usage=${AOS_STACK_USAGE})
target_include_directories(${TARGET} PUBLIC ${AOS_CORE_LIB_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/include)

# ######################################################################################################################
# Install
# ######################################################################################################################

install(TARGETS ${TARGET} ARCHIVE DESTINATION lib)

install(
    DIRECTORY ${AOS_CORE_LIB_DIR}/include/aos/common
    DESTINATION include/aos
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h"
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/aos/common/version.hpp DESTINATION include/aos/common)
