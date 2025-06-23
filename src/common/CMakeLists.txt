#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aoscommon)

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

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ######################################################################################################################
# Add subdirectories
# ######################################################################################################################

add_subdirectory(crypto)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(
    ${TARGET} STATIC
    monitoring/average.cpp
    monitoring/alertprocessor.cpp
    monitoring/resourcemonitor.cpp
    pkcs11/pkcs11.cpp
    pkcs11/privatekey.cpp
    tools/fs.cpp
    tools/semver.cpp
    tools/time.cpp
    tools/timer.cpp
    tools/uuid.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/include/aos/common/version.hpp
)

target_compile_options(${TARGET} PRIVATE -Wstack-usage=${AOS_STACK_USAGE})
target_include_directories(${TARGET} PUBLIC ${AOS_CORE_LIB_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(${TARGET} PUBLIC aoscrypto)

# ######################################################################################################################
# Install
# ######################################################################################################################

install(TARGETS ${TARGET} aoscrypto ARCHIVE DESTINATION lib)

install(
    DIRECTORY ${AOS_CORE_LIB_DIR}/include/aos/common
    DESTINATION include/aos
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h"
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/aos/common/version.hpp DESTINATION include/aos/common)
