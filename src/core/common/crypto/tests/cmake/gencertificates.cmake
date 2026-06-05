#
# Copyright (C) 2025 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

find_program(OPENSSL_EXECUTABLE openssl REQUIRED)

# Helper for running OpenSSL with proper logging
function(run_openssl)
    execute_process(
        COMMAND ${OPENSSL_EXECUTABLE} ${ARGV}
        WORKING_DIRECTORY "${OUTPUT_DIR}"
        RESULT_VARIABLE res
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )
    if(NOT res EQUAL 0)
        message(FATAL_ERROR "OpenSSL command failed: ${ARGV}\n${stdout}\n${stderr}")
    endif()
endfunction()

#
# GenCertificates function generates:
#  CA artifacts: ca.pem & ca.key in PEM format, ca.cer.der in DER format
#  Client artifacts: client.cer & client.key in PEM, client.cer.der in DER format
#  Client-CA certificate chain: client-ca-chain.pem
#

# Generate child certificate
function(genchildcert CERTIFICATES_DIR FILE_NAME COMMONNAME PARENT_NAME)
    set(OUTPUT_DIR "${CERTIFICATES_DIR}")

    message(
        STATUS "Generating child certificate '${FILE_NAME}' with CN='${COMMONNAME}' using parent '${PARENT_NAME}'..."
    )

    run_openssl(
        req
        -new
        -keyout
        "${CERTIFICATES_DIR}/${FILE_NAME}.key"
        -out
        "${CERTIFICATES_DIR}/${FILE_NAME}.csr"
        -nodes
        -subj
        "/CN=${COMMONNAME}"
    )

    run_openssl(
        x509
        -req
        -days
        365
        -extfile
        "${CERTIFICATES_DIR}/extensions.conf"
        -in
        "${CERTIFICATES_DIR}/${FILE_NAME}.csr"
        -CA
        "${CERTIFICATES_DIR}/${PARENT_NAME}.cer"
        -CAkey
        "${CERTIFICATES_DIR}/${PARENT_NAME}.key"
        -out
        "${CERTIFICATES_DIR}/${FILE_NAME}.cer"
    )
endfunction()

# Generate full certificate hierarchy
function(gencertificates TARGET CERTIFICATES_DIR)
    set(OUTPUT_DIR "${CERTIFICATES_DIR}")
    file(MAKE_DIRECTORY "${CERTIFICATES_DIR}")
    write_file("${CERTIFICATES_DIR}/extensions.conf" "basicConstraints = CA:TRUE\n")

    message(STATUS "Generating PKCS11 test certificates in: ${CERTIFICATES_DIR}")

    message(STATUS "Creating Certificate Authority private key and CSR...")
    run_openssl(
        req
        -new
        -newkey
        rsa:2048
        -nodes
        -out
        "${CERTIFICATES_DIR}/ca.csr"
        -keyout
        "${CERTIFICATES_DIR}/ca.key"
        -set_serial
        42
        -subj
        "/CN=Aos Cloud"
    )

    message(STATUS "Creating CA self-signed certificate...")
    run_openssl(
        x509
        -extfile
        "${CERTIFICATES_DIR}/extensions.conf"
        -signkey
        "${CERTIFICATES_DIR}/ca.key"
        -days
        365
        -req
        -in
        "${CERTIFICATES_DIR}/ca.csr"
        -out
        "${CERTIFICATES_DIR}/ca.cer"
    )

    message(STATUS "Issuing client certificates...")
    genchildcert("${CERTIFICATES_DIR}" "client_int" "client_int" "ca")
    genchildcert("${CERTIFICATES_DIR}" "client" "client" "client_int")

    message(STATUS "Issuing server certificates...")
    genchildcert("${CERTIFICATES_DIR}" "server_int" "server_int" "ca")
    genchildcert("${CERTIFICATES_DIR}" "server" "localhost" "server_int")

    target_compile_definitions(${TARGET} PUBLIC TEST_CERTIFICATES_DIR="${CERTIFICATES_DIR}")
endfunction()

function(generate_cryptohelper_test_certs TARGET OUTPUT_DIR)
    set(DEFAULT_EXT
        "basicConstraints=CA:FALSE
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth,clientAuth
subjectAltName=DNS:localhost,DNS:wwwaosum,DNS:*.kyiv.epam.com,DNS:*.aos-dev.test,IP:127.0.0.1,IP:::1"
    )

    set(ONLINE_CERT_EXT
        "basicConstraints=CA:FALSE
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth,clientAuth
subjectAltName=DNS:localhost,DNS:wwwaosum,DNS:*.kyiv.epam.com,DNS:*.aos-dev.test,IP:127.0.0.1,IP:::1
issuerAltName=URI:https://www.mytest.com"
    )

    function(write_ext_file NAME CONTENT)
        set(path "${OUTPUT_DIR}/${NAME}.ext")
        file(WRITE "${path}" "${CONTENT}\n")
        set(${NAME}_EXTFILE
            "${path}"
            PARENT_SCOPE
        )
    endfunction()

    function(make_leaf NAME SUBJECT EXTENSION)
        message(STATUS "Generating ${NAME} certificate key and CSR...")
        run_openssl(
            req
            -new
            -newkey
            rsa:2048
            -nodes
            -keyout
            ${NAME}.key
            -out
            ${NAME}.csr
            -subj
            "${SUBJECT}"
        )
        message(STATUS "Signing ${NAME} CSR with Intermediate CA...")
        write_ext_file(${NAME} ${EXTENSION})
        run_openssl(
            x509
            -req
            -in
            ${NAME}.csr
            -CA
            intermediateCA.pem
            -CAkey
            intermediateCA.key
            -CAcreateserial
            -out
            ${NAME}.pem
            -days
            3650
            -sha256
            -extfile
            "${${NAME}_EXTFILE}"
        )
    endfunction()

    file(MAKE_DIRECTORY "${OUTPUT_DIR}")

    message(STATUS "Generating Root CA...")
    run_openssl(
        req
        -x509
        -newkey
        rsa:2048
        -nodes
        -keyout
        rootCA.key
        -out
        rootCA.pem
        -sha256
        -days
        3650
        -subj
        "/C=UA/O=EPAM/OU=Aos/CN=Fusion Root CA/L=Kyiv"
        -addext
        "basicConstraints=CA:TRUE"
        -addext
        "keyUsage=digitalSignature,keyEncipherment,keyCertSign,cRLSign"
        -addext
        "extendedKeyUsage=serverAuth,clientAuth"
        -addext
        "subjectAltName=DNS:localhost,DNS:wwwaosum,DNS:*.kyiv.epam.com,DNS:*.aos-dev.test,IP:127.0.0.1,IP:::1"
    )

    message(STATUS "Generating Secondary CA key and CSR...")
    run_openssl(
        req
        -new
        -newkey
        rsa:2048
        -nodes
        -keyout
        secondaryCA.key
        -out
        secondaryCA.csr
        -subj
        "/C=UA/O=EPAM/OU=Aos/CN=AoS Secondary CA/L=Kyiv"
    )

    message(STATUS "Signing Secondary CA CSR with Root CA...")
    write_ext_file(
        secondaryCA
        "basicConstraints=CA:TRUE
keyUsage=digitalSignature,keyEncipherment,keyCertSign,cRLSign
extendedKeyUsage=serverAuth,clientAuth
subjectAltName=DNS:localhost,DNS:wwwaosum,DNS:*.kyiv.epam.com,DNS:*.aos-dev.test,IP:127.0.0.1,IP:::1"
    )
    run_openssl(
        x509
        -req
        -in
        secondaryCA.csr
        -CA
        rootCA.pem
        -CAkey
        rootCA.key
        -CAcreateserial
        -out
        secondaryCA.pem
        -days
        3650
        -sha256
        -extfile
        "${secondaryCA_EXTFILE}"
    )

    message(STATUS "Generating Intermediate CA key and CSR...")
    run_openssl(
        req
        -new
        -newkey
        rsa:2048
        -nodes
        -keyout
        intermediateCA.key
        -out
        intermediateCA.csr
        -subj
        "/C=UA/O=EPAM/OU=Novus Ordo Seclorum/CN=AOS vehicles Intermediate CA/L=Kyiv"
    )

    message(STATUS "Signing Intermediate CA CSR with Secondary CA...")
    write_ext_file(
        intermediateCA
        "basicConstraints=CA:TRUE
keyUsage=digitalSignature,keyEncipherment,keyCertSign,cRLSign
extendedKeyUsage=serverAuth,clientAuth
subjectAltName=DNS:localhost,DNS:wwwaosum,DNS:*.kyiv.epam.com,DNS:*.aos-dev.test,IP:127.0.0.1,IP:::1"
    )
    run_openssl(
        x509
        -req
        -in
        intermediateCA.csr
        -CA
        secondaryCA.pem
        -CAkey
        secondaryCA.key
        -CAcreateserial
        -out
        intermediateCA.pem
        -days
        3650
        -sha256
        -extfile
        "${intermediateCA_EXTFILE}"
    )

    make_leaf(offline2 "/C=UA/O=EPAM Systems, Inc./OU=Test vehicle model/CN=YV1SW58D202057528-offline/L=Kyiv"
              ${DEFAULT_EXT}
    )
    make_leaf(offline1 "/C=UA/O=EPAM Systems, Inc./OU=Test vehicle model/CN=YV1SW58D900034248-offline/L=Kyiv"
              ${DEFAULT_EXT}
    )
    make_leaf(
        online
        "/C=UA/O=staging-fusion.westeurope.cloudapp.azure.com/OU=OEM Test unit model/CN=c183de63-e2b7-4776-90e0-b7c9b8f740e8-online/L=Kyiv"
        ${ONLINE_CERT_EXT}
    )
    make_leaf(onlineTest1 "/C=UA/O=Test1/OU=OEM Test1/CN=test1/L=Kyiv" ${DEFAULT_EXT})
    make_leaf(onlineTest2 "/C=UA/OU=OEM Test2/CN=test2/L=Kyiv" ${DEFAULT_EXT})

    message(STATUS "All certificates and keys generated successfully in ${OUTPUT_DIR}")

    target_compile_definitions(${TARGET} PUBLIC CRYPTOHELPER_CERTS_DIR="${OUTPUT_DIR}")
endfunction()

function(cryptohelper_generate_test_aes TARGET OUTPUT_DIR OFFLINE_CERT)
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")

    # Create AES-256 key
    set(AES_KEY "${OUTPUT_DIR}/aes.key")
    message(STATUS "Generating random AES key...")
    run_openssl(rand -out "${AES_KEY}" 32)

    set(PUBKEY_FILE "${OUTPUT_DIR}/offline1.pubkey.pem")
    message(STATUS "Extracting public key from ${OFFLINE_CERT}...")
    run_openssl(
        x509
        -pubkey
        -noout
        -in
        "${OFFLINE_CERT}"
        -out
        "${PUBKEY_FILE}"
    )

    message(STATUS "AES key created successfully")

    target_compile_definitions(${TARGET} PUBLIC CRYPTOHELPER_AES_DIR="${OUTPUT_DIR}")
endfunction()

# Encrypts "Hello World" with AES-256-CBC and writes ciphertext into OUTPUT_FILE
function(cryptohelper_generate_encrypted_test_file AES_KEY_FILE OUTPUT_FILE)
    set(FIXED_IV "01020304050000000000000000000000") # IV = {1,2,3,4,5,0,...}

    # 1) Create plaintext file
    file(WRITE "${OUTPUT_FILE}" "Hello World\n")

    # 2) Read AES key (binary)
    file(READ "${AES_KEY_FILE}" AES_KEY_HEX HEX)

    if(NOT AES_KEY_HEX)
        message(FATAL_ERROR "Failed to read AES key from ${AES_KEY_FILE}")
    endif()

    # 3) Encrypt with AES (output is binary)
    message(STATUS "Encrypting test file ${OUTPUT_FILE} with AES key ${AES_KEY_FILE}...")

    run_openssl(
        enc
        -aes-256-cbc
        -in
        "${OUTPUT_FILE}"
        -out
        "${OUTPUT_FILE}.enc"
        -K
        "${AES_KEY_HEX}"
        -iv
        "${FIXED_IV}"
    )
endfunction()

# Sign a file using RSA/SHA256/PKCS1v1_5
function(cryptohelper_sign_file WORK_DIR INPUT_FILE PRIVATE_KEY_NAME)
    message(STATUS "Signing file: ${INPUT_FILE} with key: ${PRIVATE_KEY_NAME}.key")

    file(WRITE "${WORK_DIR}/${INPUT_FILE}" "Hello World\n")

    run_openssl(
        dgst
        -sha256
        -sign
        "${WORK_DIR}/${PRIVATE_KEY_NAME}.key"
        -out
        "${WORK_DIR}/${INPUT_FILE}.${PRIVATE_KEY_NAME}.sig"
        "${WORK_DIR}/${INPUT_FILE}"
    )
endfunction()

# Encrypt a file into CMS EnvelopedData using RSA recipient certificate
function(cryptohelper_create_cms WORK_DIR INPUT_FILE RECIPIENT_CERT_NAME)
    message(STATUS "Encrypting file: ${INPUT_FILE} for recipient: ${RECIPIENT_CERT_NAME}.pem")

    file(WRITE "${WORK_DIR}/${INPUT_FILE}" "Hello World\n")

    run_openssl(
        cms
        -encrypt
        -aes-256-cbc
        -binary
        -in
        "${WORK_DIR}/${INPUT_FILE}"
        -out
        "${WORK_DIR}/${INPUT_FILE}.${RECIPIENT_CERT_NAME}.cms"
        -outform
        DER
        "${WORK_DIR}/${RECIPIENT_CERT_NAME}.pem"
    )
endfunction()
