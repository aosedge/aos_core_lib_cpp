#
# Copyright (C) 2025 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME crypto_test)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES asn1.cpp)

if(WITH_MBEDTLS OR WITH_OPENSSL)
    list(APPEND SOURCES certloader.cpp cryptohelper.cpp cryptoprovider.cpp cryptoutils.cpp)
endif()

# ######################################################################################################################
# Libraries
# ######################################################################################################################

set(LIBRARIES aos::core::common::tests::crypto aos::core::common::tests::utils GTest::gmock_main)

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

######################################################################################################################
# Gen certificates
# ######################################################################################################################

include(cmake/gencertificates.cmake)

message("\n\n==================================================")
message("Generating certificates for CryptoProvider tests...")
message("==================================================")

gencertificates("${TARGET_PREFIX}_${TARGET_NAME}" "${CMAKE_CURRENT_BINARY_DIR}/certificates")

message("\n\n==================================================")
message("Generating certificates for CryptoHelper tests...")
message("==================================================")

generate_cryptohelper_test_certs("${TARGET_PREFIX}_${TARGET_NAME}" "${CMAKE_CURRENT_BINARY_DIR}/cryptohelper")

cryptohelper_generate_test_aes(
    "${TARGET_PREFIX}_${TARGET_NAME}" "${CMAKE_CURRENT_BINARY_DIR}/cryptohelper/aes"
    "${CMAKE_CURRENT_BINARY_DIR}/cryptohelper/offline1.pem"
)
cryptohelper_generate_encrypted_test_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cryptohelper/aes/aes.key"
    "${CMAKE_CURRENT_BINARY_DIR}/cryptohelper/aes/hello-world.txt"
)

cryptohelper_sign_file("${CMAKE_CURRENT_BINARY_DIR}/cryptohelper" "hello-world.txt" "online")
cryptohelper_sign_file("${CMAKE_CURRENT_BINARY_DIR}/cryptohelper" "hello-world.txt" "offline1")
cryptohelper_sign_file("${CMAKE_CURRENT_BINARY_DIR}/cryptohelper" "hello-world.txt" "offline2")
cryptohelper_sign_file("${CMAKE_CURRENT_BINARY_DIR}/cryptohelper" "hello-world.txt" "onlineTest1")
cryptohelper_sign_file("${CMAKE_CURRENT_BINARY_DIR}/cryptohelper" "hello-world.txt" "onlineTest2")

cryptohelper_create_cms("${CMAKE_CURRENT_BINARY_DIR}/cryptohelper" "hello-world-cms.txt" "offline1")
