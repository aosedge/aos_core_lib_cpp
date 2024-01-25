/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_COMMON_CONFIG_HPP_
#define AOS_COMMON_CONFIG_HPP_

/**
 * Service provider ID len.
 */
#ifndef AOS_CONFIG_TYPES_PROVIDER_ID_LEN
#define AOS_CONFIG_TYPES_PROVIDER_ID_LEN 40
#endif

/**
 * Service ID len.
 */
#ifndef AOS_CONFIG_TYPES_SERVICE_ID_LEN
#define AOS_CONFIG_TYPES_SERVICE_ID_LEN 40
#endif

/**
 * Subject ID len.
 */
#ifndef AOS_CONFIG_TYPES_SUBJECT_ID_LEN
#define AOS_CONFIG_TYPES_SUBJECT_ID_LEN 40
#endif

/**
 * Layer ID len.
 */
#ifndef AOS_CONFIG_TYPES_LAYER_ID_LEN
#define AOS_CONFIG_TYPES_LAYER_ID_LEN 40
#endif

/**
 * Layer digest len.
 */
#ifndef AOS_CONFIG_TYPES_LAYER_DIGEST_LEN
#define AOS_CONFIG_TYPES_LAYER_DIGEST_LEN 128
#endif

/**
 * Instance ID len.
 */
#ifndef AOS_CONFIG_TYPES_INSTANCE_ID_LEN
#define AOS_CONFIG_TYPES_INSTANCE_ID_LEN 40
#endif

/**
 * URL len.
 */
#ifndef AOS_CONFIG_TYPES_URL_LEN
#define AOS_CONFIG_TYPES_URL_LEN 256
#endif

/**
 * Vendor version len.
 */
#ifndef AOS_CONFIG_TYPES_VENDOR_VERSION_LEN
#define AOS_CONFIG_TYPES_VENDOR_VERSION_LEN 20
#endif

/**
 * Service/layer description len.
 */
#ifndef AOS_CONFIG_TYPES_DESCRIPTION_LEN
#define AOS_CONFIG_TYPES_DESCRIPTION_LEN 200
#endif

/**
 * Max number of services.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_SERVICES
#define AOS_CONFIG_TYPES_MAX_NUM_SERVICES 16
#endif

/**
 * Max number of layers.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_LAYERS
#define AOS_CONFIG_TYPES_MAX_NUM_LAYERS 16
#endif

/**
 * Max number of instances.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_INSTANCES
#define AOS_CONFIG_TYPES_MAX_NUM_INSTANCES 16
#endif

/**
 * Error message len.
 */
#ifndef AOS_CONFIG_TYPES_ERROR_MESSAGE_LEN
#define AOS_CONFIG_TYPES_ERROR_MESSAGE_LEN 256
#endif

/**
 * File chunk size.
 */
#ifndef AOS_CONFIG_TYPES_FILE_CHUNK_SIZE
#define AOS_CONFIG_TYPES_FILE_CHUNK_SIZE 1024
#endif

/**
 * Max number of partitions.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_PARTITIONS
#define AOS_CONFIG_TYPES_MAX_NUM_PARTITIONS 4
#endif

/**
 * Max number of partition types.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_PARTITION_TYPES
#define AOS_CONFIG_TYPES_MAX_NUM_PARTITION_TYPES 4
#endif

/**
 * Partition name len.
 */
#ifndef AOS_CONFIG_TYPES_PARTITION_NAME_LEN
#define AOS_CONFIG_TYPES_PARTITION_NAME_LEN 64
#endif

/**
 * Partition types len.
 */
#ifndef AOS_CONFIG_TYPES_PARTITION_TYPES_LEN
#define AOS_CONFIG_TYPES_PARTITION_TYPES_LEN 32
#endif

/**
 * Node ID len.
 */
#ifndef AOS_CONFIG_TYPES_NODE_ID_LEN
#define AOS_CONFIG_TYPES_NODE_ID_LEN 64
#endif

/**
 * Node type len.
 */
#ifndef AOS_CONFIG_TYPES_NODE_TYPE_LEN
#define AOS_CONFIG_TYPES_NODE_TYPE_LEN 64
#endif

/**
 * Monitoring send period.
 */
#ifndef AOS_CONFIG_MONITORING_SEND_PERIOD_SEC
#define AOS_CONFIG_MONITORING_SEND_PERIOD_SEC 60
#endif

/**
 * Monitoring poll period.
 */
#ifndef AOS_CONFIG_MONITORING_POLL_PERIOD_SEC
#define AOS_CONFIG_MONITORING_POLL_PERIOD_SEC 10
#endif

/**
 * Certificate issuer max size is not specified in general.
 * (RelativeDistinguishedName ::= SET SIZE (1..MAX) OF AttributeTypeAndValue)
 */
#ifndef AOS_CONFIG_CRYPTO_CERT_ISSUER_SIZE
#define AOS_CONFIG_CRYPTO_CERT_ISSUER_SIZE 128
#endif

/**
 * Maximum length of distinguished name string representation.
 */
#ifndef AOS_CONFIG_CRYPTO_DN_STRING_SIZE
#define AOS_CONFIG_CRYPTO_DN_STRING_SIZE 128
#endif

/**
 * Max length of a DNS name.
 */
#ifndef AOS_CONFIG_CRYPTO_DNS_NAME_LEN
#define AOS_CONFIG_CRYPTO_DNS_NAME_LEN 42
#endif

/**
 * Max number of alternative DNS names for a module.
 */
#ifndef AOS_CONFIG_CRYPTO_ALT_DNS_NAMES_MAX_COUNT
#define AOS_CONFIG_CRYPTO_ALT_DNS_NAMES_MAX_COUNT 4
#endif

/**
 * Raw certificate request size.
 */
#ifndef AOS_CONFIG_CRYPTO_EXTRA_EXTENSIONS_COUNT
#define AOS_CONFIG_CRYPTO_EXTRA_EXTENSIONS_COUNT 10
#endif

/**
 * Maximum length of numeric string representing ASN.1 Object Identifier.
 */
#ifndef AOS_CONFIG_CRYPTO_ASN1_OBJECT_ID_LEN
#define AOS_CONFIG_CRYPTO_ASN1_OBJECT_ID_LEN 24
#endif

/**
 * Maximum size of a certificate ASN.1 Extension Value.
 */
#ifndef AOS_CONFIG_CRYPTO_ASN1_EXTENSION_VALUE_SIZE
#define AOS_CONFIG_CRYPTO_ASN1_EXTENSION_VALUE_SIZE 16
#endif

/**
 * Maximum certificate key id size(in bytes).
 */
#ifndef AOS_CONFIG_CRYPTO_CERT_KEY_ID_SIZE
#define AOS_CONFIG_CRYPTO_CERT_KEY_ID_SIZE 24
#endif

/**
 * Maximum size of a PEM certificate.
 */
#ifndef AOS_CONFIG_CRYPTO_CERT_PEM_SIZE
#define AOS_CONFIG_CRYPTO_CERT_PEM_SIZE 4096
#endif

/**
 * Maximum size of a DER certificate.
 */
#ifndef AOS_CONFIG_CRYPTO_CERT_DER_SIZE
#define AOS_CONFIG_CRYPTO_CERT_DER_SIZE 2048
#endif

/**
 * Maximum size of CSR in PEM format.
 */
#ifndef AOS_CONFIG_CRYPTO_CSR_PEM_SIZE
#define AOS_CONFIG_CRYPTO_CSR_PEM_SIZE 4096
#endif

/**
 * Serial number size(in bytes).
 */
#ifndef AOS_CONFIG_CRYPTO_SERIAL_NUM_SIZE
#define AOS_CONFIG_CRYPTO_SERIAL_NUM_SIZE 20
#endif

/**
 * Subject common name length.
 */
#ifndef AOS_CONFIG_CRYPTO_SUBJECT_COMMON_NAME_LEN
#define AOS_CONFIG_CRYPTO_SUBJECT_COMMON_NAME_LEN 256
#endif

/**
 * Usual RSA modulus size is 512, 1024, 2048 or 4096 bit length.
 */
#ifndef AOS_CONFIG_CRYPTO_RSA_MODULUS_SIZE
#define AOS_CONFIG_CRYPTO_RSA_MODULUS_SIZE 512
#endif

/**
 * In general field length of a public exponent (e) is typically 1, 3, or 64 - 512 bytes.
 */
#ifndef AOS_CONFIG_CRYPTO_RSA_PUB_EXPONENT_SIZE
#define AOS_CONFIG_CRYPTO_RSA_PUB_EXPONENT_SIZE 3
#endif

/**
 * Size of ECDSA params OID.
 */
#ifndef AOS_CONFIG_CRYPTO_ECDSA_PARAMS_OID_SIZE
#define AOS_CONFIG_CRYPTO_ECDSA_PARAMS_OID_SIZE 10
#endif

/**
 * DER-encoded X9.62 ECPoint size.
 */
#ifndef AOS_CONFIG_CRYPTO_ECDSA_POINT_DER_SIZE
#define AOS_CONFIG_CRYPTO_ECDSA_POINT_DER_SIZE 128
#endif

/**
 * Maximum size of SHA2 digest.
 */
#ifndef AOS_CONFIG_CRYPTO_SHA2_DIGEST_SIZE
#define AOS_CONFIG_CRYPTO_SHA2_DIGEST_SIZE 512
#endif

/**
 * Maximum size of signature.
 */
#ifndef AOS_CONFIG_CRYPTO_SIGNATURE_SIZE
#define AOS_CONFIG_CRYPTO_SIGNATURE_SIZE 512
#endif

/**
 * Maximum length of PKCS11 slot description.
 */
#ifndef AOS_CONFIG_PKCS11_SLOT_DESCRIPTION_LEN
#define AOS_CONFIG_PKCS11_SLOT_DESCRIPTION_LEN 64
#endif

/**
 * Maximum length of PKCS11 manufacture ID.
 */
#ifndef AOS_CONFIG_PKCS11_MANUFACTURE_ID_LEN
#define AOS_CONFIG_PKCS11_MANUFACTURE_ID_LEN 32
#endif

/**
 * Maximum length of PKCS11 label.
 */
#ifndef AOS_CONFIG_PKCS11_LABEL_LEN
#define AOS_CONFIG_PKCS11_LABEL_LEN 32
#endif

/**
 * PKCS11 library description length.
 */
#ifndef AOS_CONFIG_PKCS11_LIBRARY_DESC_LEN
#define AOS_CONFIG_PKCS11_LIBRARY_DESC_LEN 64
#endif

/**
 * HSM device model length.
 */
#ifndef AOS_CONFIG_PKCS11_MODEL_LEN
#define AOS_CONFIG_PKCS11_MODEL_LEN 16
#endif

/**
 * Maximum length of user PIN (password).
 */
#ifndef AOS_CONFIG_PKCS11_PIN_LEN
#define AOS_CONFIG_PKCS11_PIN_LEN 42
#endif

/**
 * Maximum number of open sessions per PKCS11 library.
 */
#ifndef AOS_CONFIG_PKCS11_SESSIONS_PER_LIB
#define AOS_CONFIG_PKCS11_SESSIONS_PER_LIB 3
#endif

/**
 * Maximum number of attributes for object.
 */
#ifndef AOS_CONFIG_PKCS11_OBJECT_ATTRIBUTES_COUNT
#define AOS_CONFIG_PKCS11_OBJECT_ATTRIBUTES_COUNT 10
#endif

/**
 * Maximum number of keys per token.
 */
#ifndef AOS_CONFIG_PKCS11_TOKEN_KEYS_COUNT
#define AOS_CONFIG_PKCS11_TOKEN_KEYS_COUNT 20
#endif

/**
 * Maximum number of PKCS11 libraries.
 */
#ifndef AOS_CONFIG_PKCS11_MAX_NUM_LIBRARIES
#define AOS_CONFIG_PKCS11_MAX_NUM_LIBRARIES 1
#endif

/**
 * Maximum size of slot list.
 */
#ifndef AOS_CONFIG_PKCS11_SLOT_LIST_SIZE
#define AOS_CONFIG_PKCS11_SLOT_LIST_SIZE 5
#endif

/**
 * Flag to choose between static/dynamic pkcs11 library.
 */
#ifndef AOS_CONFIG_PKCS11_USE_STATIC_LIB
#define AOS_CONFIG_PKCS11_USE_STATIC_LIB 0
#endif

/**
 *  UUID length
 */
#ifndef AOS_CONFIG_UUID_LEN
#define AOS_CONFIG_UUID_LEN 16
#endif

/**
 *  Length of UUID string representation
 */
#ifndef AOS_CONFIG_UUID_STR_LEN
#define AOS_CONFIG_UUID_STR_LEN AOS_CONFIG_UUID_LEN * 2 + 4 + 1 // 32 hex digits + 4 '-' symbols + '\0'
#endif

/**
 * Max expected number of certificates in a chain stored in PEM file.
 */
#ifndef AOS_CONFIG_CRYPTO_CERTS_CHAIN_SIZE
#define AOS_CONFIG_CRYPTO_CERTS_CHAIN_SIZE 3
#endif

/**
 * Capacity (in bytes) of cryptoutils::CertLoader.
 */
#ifndef AOS_CONFIG_CRYPTOUTILS_CERTLOADER_CAPACITY
#define AOS_CONFIG_CRYPTOUTILS_CERTLOADER_CAPACITY                                                                     \
    AOS_CONFIG_CRYPTO_CERTS_CHAIN_SIZE* AOS_CONFIG_CRYPTO_CERT_DER_SIZE * 5
#endif

/**
 * Default PKCS11 library.
 */
#ifndef AOS_CONFIG_CRYPTOUTILS_DEFAULT_PKCS11_LIB
#define AOS_CONFIG_CRYPTOUTILS_DEFAULT_PKCS11_LIB "/usr/lib/softhsm/libsofthsm2.so"
#endif

#endif
