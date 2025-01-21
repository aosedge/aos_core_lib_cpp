/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_COMMON_CONFIG_HPP_
#define AOS_COMMON_CONFIG_HPP_

/**
 * Max error message len.
 */
#ifndef AOS_CONFIG_TOOLS_ERROR_MESSAGE_LEN
#define AOS_CONFIG_TOOLS_ERROR_MESSAGE_LEN 64
#endif

/**
 * Max error print message len.
 */
#ifndef AOS_CONFIG_TOOLS_ERROR_STR_LEN
#define AOS_CONFIG_TOOLS_ERROR_STR_LEN 128
#endif

/**
 *  UUID size.
 */
#ifndef AOS_CONFIG_TOOLS_UUID_SIZE
#define AOS_CONFIG_TOOLS_UUID_SIZE 16
#endif

/**
 *  Length of UUID string representation.
 */
#ifndef AOS_CONFIG_TOOLS_UUID_LEN
#define AOS_CONFIG_TOOLS_UUID_LEN AOS_CONFIG_TOOLS_UUID_SIZE * 2 + 4 + 1 // 32 hex digits + 4 '-' symbols + '\0'
#endif

/**
 * Service provider ID len.
 */
#ifndef AOS_CONFIG_TYPES_PROVIDER_ID_LEN
#define AOS_CONFIG_TYPES_PROVIDER_ID_LEN 40
#endif

/**
 * Max number of service providers.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_SERVICE_PROVIDERS
#define AOS_CONFIG_TYPES_MAX_NUM_SERVICE_PROVIDERS 4
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
 * Max expected number of subject ids.
 */
#ifndef AOS_CONFIG_TYPES_MAX_SUBJECTS_SIZE
#define AOS_CONFIG_TYPES_MAX_SUBJECTS_SIZE 4
#endif

/**
 * System ID len.
 */
#ifndef AOS_CONFIG_TYPES_SYSTEM_ID_LEN
#define AOS_CONFIG_TYPES_SYSTEM_ID_LEN 40
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
 * Unit model len.
 */
#ifndef AOS_CONFIG_TYPES_UNIT_MODEL_LEN
#define AOS_CONFIG_TYPES_UNIT_MODEL_LEN 40
#endif

/**
 * URL len.
 */
#ifndef AOS_CONFIG_TYPES_URL_LEN
#define AOS_CONFIG_TYPES_URL_LEN 256
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
#define AOS_CONFIG_TYPES_MAX_NUM_SERVICES 64
#endif

/**
 * Max number of layers.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_LAYERS
#define AOS_CONFIG_TYPES_MAX_NUM_LAYERS 64
#endif

/**
 * Max number of instances.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_INSTANCES
#define AOS_CONFIG_TYPES_MAX_NUM_INSTANCES 256
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
 * File system mount type len.
 */
#ifndef AOS_CONFIG_TYPES_FS_MOUNT_TYPE_LEN
#define AOS_CONFIG_TYPES_FS_MOUNT_TYPE_LEN 16
#endif

/**
 * File system mount option len.
 */
#ifndef AOS_CONFIG_TYPES_FS_MOUNT_OPTION_LEN
#define AOS_CONFIG_TYPES_FS_MOUNT_OPTION_LEN 16
#endif

/**
 * Max number of file system mount options.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_FS_MOUNT_OPTIONS
#define AOS_CONFIG_TYPES_MAX_NUM_FS_MOUNT_OPTIONS 8
#endif

/**
 * IP len.
 */
#ifndef AOS_CONFIG_TYPES_IP_LEN
#define AOS_CONFIG_TYPES_IP_LEN 48
#endif

/**
 * Port len.
 */
#ifndef AOS_CONFIG_TYPES_PORT_LEN
#define AOS_CONFIG_TYPES_PORT_LEN 8
#endif

/**
 * Protocol name len.
 */
#ifndef AOS_CONFIG_TYPES_PROTOCOL_NAME_LEN
#define AOS_CONFIG_TYPES_PROTOCOL_NAME_LEN 6
#endif

/**
 * Max number of DNS servers.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_DNS_SERVERS
#define AOS_CONFIG_TYPES_MAX_NUM_DNS_SERVERS 4
#endif

/**
 * Max number of firewall rules.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_FIREWALL_RULES
#define AOS_CONFIG_TYPES_MAX_NUM_FIREWALL_RULES 10
#endif

/**
 * Max number of networks.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_NETWORKS
#define AOS_CONFIG_TYPES_MAX_NUM_NETWORKS 4
#endif

/**
 * Host name len.
 */
#ifndef AOS_CONFIG_TYPES_HOST_NAME_LEN
#define AOS_CONFIG_TYPES_HOST_NAME_LEN 128
#endif

/**
 * Host device name len.
 */
#ifndef AOS_CONFIG_TYPES_DEVICE_NAME_LEN
#define AOS_CONFIG_TYPES_DEVICE_NAME_LEN 16
#endif

/**
 * Max number of host devices.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_HOST_DEVICES
#define AOS_CONFIG_TYPES_MAX_NUM_HOST_DEVICES 32
#endif

/**
 * Resource name len.
 */
#ifndef AOS_CONFIG_TYPES_RESOURCE_NAME_LEN
#define AOS_CONFIG_TYPES_RESOURCE_NAME_LEN 16
#endif

/**
 * Group name len.
 */
#ifndef AOS_CONFIG_TYPES_GROUP_NAME_LEN
#define AOS_CONFIG_TYPES_GROUP_NAME_LEN 32
#endif

/**
 * Max number of groups.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_GROUPS
#define AOS_CONFIG_TYPES_MAX_NUM_GROUPS 32
#endif

/**
 * Max number of groups.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_GROUPS
#define AOS_CONFIG_TYPES_MAX_NUM_GROUPS 32
#endif

/**
 * Max number of file system mounts.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_FS_MOUNTS
#define AOS_CONFIG_TYPES_MAX_NUM_FS_MOUNTS 16
#endif

/**
 * Environment variable name len.
 */
#ifndef AOS_CONFIG_TYPES_ENV_VAR_NAME_LEN
#define AOS_CONFIG_TYPES_ENV_VAR_NAME_LEN 256
#endif

/**
 * Max number of environment variables.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_ENV_VARIABLES
#define AOS_CONFIG_TYPES_MAX_NUM_ENV_VARIABLES 16
#endif

/**
 * Max number of hosts.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_HOSTS
#define AOS_CONFIG_TYPES_MAX_NUM_HOSTS 4
#endif

/**
 * Max number of node's devices.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_NODE_DEVICES
#define AOS_CONFIG_TYPES_MAX_NUM_NODE_DEVICES 8
#endif

/**
 * Max number of node's resources.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_NODE_RESOURCES
#define AOS_CONFIG_TYPES_MAX_NUM_NODE_RESOURCES 4
#endif

/**
 * Label name len.
 */
#ifndef AOS_CONFIG_TYPES_LABEL_NAME_LEN
#define AOS_CONFIG_TYPES_LABEL_NAME_LEN 16
#endif

/**
 * Max number of node's labels.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_NODE_LABELS
#define AOS_CONFIG_TYPES_MAX_NUM_NODE_LABELS 4
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
 * Max number of nodes.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_NODES
#define AOS_CONFIG_TYPES_MAX_NUM_NODES 16
#endif

/**
 * Node ID len.
 */
#ifndef AOS_CONFIG_TYPES_NODE_ID_LEN
#define AOS_CONFIG_TYPES_NODE_ID_LEN 64
#endif

/**
 * Node name len.
 */
#ifndef AOS_CONFIG_TYPES_NODE_NAME_LEN
#define AOS_CONFIG_TYPES_NODE_NAME_LEN 64
#endif

/**
 * Node type len.
 */
#ifndef AOS_CONFIG_TYPES_NODE_TYPE_LEN
#define AOS_CONFIG_TYPES_NODE_TYPE_LEN 64
#endif

/**
 * OS type len.
 */
#ifndef AOS_CONFIG_TYPES_OS_TYPE_LEN
#define AOS_CONFIG_TYPES_OS_TYPE_LEN 16
#endif

/**
 * Max number of CPUs.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_CPUS
#define AOS_CONFIG_TYPES_MAX_NUM_CPUS 8
#endif

/**
 * CPU model name len.
 */
#ifndef AOS_CONFIG_TYPES_CPU_MODEL_NAME_LEN
#define AOS_CONFIG_TYPES_CPU_MODEL_NAME_LEN 64
#endif

/**
 * CPU architecture len.
 */
#ifndef AOS_CONFIG_TYPES_CPU_ARCH_LEN
#define AOS_CONFIG_TYPES_CPU_ARCH_LEN 16
#endif

/**
 * CPU architecture family len.
 */
#ifndef AOS_CONFIG_TYPES_CPU_ARCH_FAMILY_LEN
#define AOS_CONFIG_TYPES_CPU_ARCH_FAMILY_LEN 16
#endif

/**
 * Node attribute name len.
 */
#ifndef AOS_CONFIG_TYPES_NODE_ATTRIBUTE_NAME_LEN
#define AOS_CONFIG_TYPES_NODE_ATTRIBUTE_NAME_LEN 16
#endif

/**
 * Node attribute value len.
 */
#ifndef AOS_CONFIG_TYPES_NODE_ATTRIBUTE_VALUE_LEN
#define AOS_CONFIG_TYPES_NODE_ATTRIBUTE_VALUE_LEN 64
#endif

/**
 * Max number of node attributes.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_NODE_ATTRIBUTES
#define AOS_CONFIG_TYPES_MAX_NUM_NODE_ATTRIBUTES 16
#endif

/**
 * Version max len.
 */
#ifndef AOS_CONFIG_TYPES_VERSION_LEN
#define AOS_CONFIG_TYPES_VERSION_LEN 32
#endif

/**
 * Max subnet length.
 */
#ifndef AOS_CONFIG_TYPES_SUBNET_LEN
#define AOS_CONFIG_TYPES_SUBNET_LEN 50
#endif

/**
 * Max MAC length.
 */
#ifndef AOS_CONFIG_TYPES_MAC_LEN
#define AOS_CONFIG_TYPES_MAC_LEN 18
#endif

/**
 * Max iptables chain name length.
 */
#ifndef AOS_CONFIG_TYPES_IPTABLES_CHAIN_LEN
#define AOS_CONFIG_TYPES_IPTABLES_CHAIN_LEN 64
#endif

/**
 * Max CNI interface name length.
 */
#ifndef AOS_CONFIG_TYPES_INTERFACE_NAME_LEN
#define AOS_CONFIG_TYPES_INTERFACE_NAME_LEN 64
#endif

/**
 * Max num runners.
 */
#ifndef AOS_CONFIG_TYPES_MAX_NUM_RUNNERS
#define AOS_CONFIG_TYPES_MAX_NUM_RUNNERS 4
#endif

/**
 * Runner name max length.
 */
#ifndef AOS_CONFIG_TYPES_RUNNER_NAME_LEN
#define AOS_CONFIG_TYPES_RUNNER_NAME_LEN 16
#endif

/**
 * Function name length.
 */
#ifndef AOS_CONFIG_TYPES_FUNCTION_LEN
#define AOS_CONFIG_TYPES_FUNCTION_LEN 32
#endif

/**
 * Permissions length.
 */
#ifndef AOS_CONFIG_TYPES_PERMISSIONS_LEN
#define AOS_CONFIG_TYPES_PERMISSIONS_LEN 32
#endif

/**
 * Maximum number of functions for functional service.
 */
#ifndef AOS_CONFIG_TYPES_FUNCTIONS_MAX_COUNT
#define AOS_CONFIG_TYPES_FUNCTIONS_MAX_COUNT 32
#endif

/**
 * Functional service name length.
 */
#ifndef AOS_CONFIG_TYPES_FUNC_SERVICE_LEN
#define AOS_CONFIG_TYPES_FUNC_SERVICE_LEN 32
#endif

/**
 * Maximum number of functional services.
 */
#ifndef AOS_CONFIG_TYPES_FUNC_SERVICE_MAX_COUNT
#define AOS_CONFIG_TYPES_FUNC_SERVICE_MAX_COUNT 16
#endif

/**
 * Max number of outdated items.
 */
#ifndef AOS_CONFIG_TYPES_MAX_OUTDATED_ITEMS
#define AOS_CONFIG_TYPES_MAX_OUTDATED_ITEMS 40
#endif

/**
 * Monitoring poll period.
 */
#ifndef AOS_CONFIG_MONITORING_POLL_PERIOD_SEC
#define AOS_CONFIG_MONITORING_POLL_PERIOD_SEC 10
#endif

/**
 * Monitoring average window.
 */
#ifndef AOS_CONFIG_MONITORING_AVERAGE_WINDOW_SEC
#define AOS_CONFIG_MONITORING_AVERAGE_WINDOW_SEC 60
#endif

/**
 * Certificate issuer max size is not specified in general.
 * (RelativeDistinguishedName ::= SET SIZE (1..MAX) OF AttributeTypeAndValue)
 */
#ifndef AOS_CONFIG_CRYPTO_CERT_ISSUER_SIZE
#define AOS_CONFIG_CRYPTO_CERT_ISSUER_SIZE 256
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
#define AOS_CONFIG_CRYPTO_ASN1_EXTENSION_VALUE_SIZE 24
#endif

/**
 * Maximum certificate key id size(in bytes).
 */
#ifndef AOS_CONFIG_CRYPTO_CERT_KEY_ID_SIZE
#define AOS_CONFIG_CRYPTO_CERT_KEY_ID_SIZE 24
#endif

/**
 * Maximum length of a PEM certificate.
 */
#ifndef AOS_CONFIG_CRYPTO_CERT_PEM_LEN
#define AOS_CONFIG_CRYPTO_CERT_PEM_LEN 4096
#endif

/**
 * Maximum size of a DER certificate.
 */
#ifndef AOS_CONFIG_CRYPTO_CERT_DER_SIZE
#define AOS_CONFIG_CRYPTO_CERT_DER_SIZE 2048
#endif

/**
 * Maximum length of CSR in PEM format.
 */
#ifndef AOS_CONFIG_CRYPTO_CSR_PEM_LEN
#define AOS_CONFIG_CRYPTO_CSR_PEM_LEN 4096
#endif

/**
 * Maximum length of private key in PEM format.
 */
#ifndef AOS_CONFIG_CRYPTO_PRIVKEY_PEM_LEN
#define AOS_CONFIG_CRYPTO_PRIVKEY_PEM_LEN 2048
#endif

/**
 * Serial number size(in bytes).
 */
#ifndef AOS_CONFIG_CRYPTO_SERIAL_NUM_SIZE
#define AOS_CONFIG_CRYPTO_SERIAL_NUM_SIZE 20
#endif

/**
 * Maximum size of serial number encoded in DER format(in bytes).
 */
#ifndef AOS_CONFIG_CRYPTO_SERIAL_NUM_DER_SIZE
#define AOS_CONFIG_CRYPTO_SERIAL_NUM_DER_SIZE AOS_CONFIG_CRYPTO_SERIAL_NUM_SIZE + 3;
#endif

/**
 * Subject common name length.
 */
#ifndef AOS_CONFIG_CRYPTO_SUBJECT_COMMON_NAME_LEN
#define AOS_CONFIG_CRYPTO_SUBJECT_COMMON_NAME_LEN 32
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
 * Maximum size of SHA1 digest.
 *
 * 128 chars to represent sha512 + 8 chars for algorithm.
 */
#ifndef AOS_CONFIG_CRYPTO_SHA1_DIGEST_SIZE
#define AOS_CONFIG_CRYPTO_SHA1_DIGEST_SIZE 136
#endif

/**
 * Maximum size of SHA2 digest.
 */
#ifndef AOS_CONFIG_CRYPTO_SHA2_DIGEST_SIZE
#define AOS_CONFIG_CRYPTO_SHA2_DIGEST_SIZE 64
#endif

/**
 * Maximum size of SHA1 digest.
 */
#ifndef AOS_CONFIG_CRYPTO_SHA1_DIGEST_SIZE
#define AOS_CONFIG_CRYPTO_SHA1_DIGEST_SIZE 20
#endif

/**
 * Maximum size of input data for SHA1 hash calculation.
 */
#ifndef AOS_CONFIG_CRYPTO_SHA1_INPUT_SIZE
#define AOS_CONFIG_CRYPTO_SHA1_INPUT_SIZE 42
#endif

/**
 * Maximum size of signature.
 */
#ifndef AOS_CONFIG_CRYPTO_SIGNATURE_SIZE
#define AOS_CONFIG_CRYPTO_SIGNATURE_SIZE 512
#endif

/**
 * Max expected number of certificates in a chain stored in PEM file.
 */
#ifndef AOS_CONFIG_CRYPTO_CERTS_CHAIN_SIZE
#define AOS_CONFIG_CRYPTO_CERTS_CHAIN_SIZE 4
#endif

/**
 * Number of certificate chains to be stored in crypto::CertLoader.
 */
#ifndef AOS_CONFIG_CRYPTO_CERTIFICATE_CHAINS_COUNT
#define AOS_CONFIG_CRYPTO_CERTIFICATE_CHAINS_COUNT 8
#endif

/**
 * Number of private keys to be stored in crypto::CertLoader.
 */
#ifndef AOS_CONFIG_CRYPTO_KEYS_COUNT
#define AOS_CONFIG_CRYPTO_KEYS_COUNT 8
#endif

/**
 * Max number of crypto allocations.
 */
#ifndef AOS_CONFIG_CRYPTO_NUM_ALLOCATIONS
#define AOS_CONFIG_CRYPTO_NUM_ALLOCATIONS 16
#endif

/**
 * Default PKCS11 library.
 */
#ifndef AOS_CONFIG_CRYPTO_DEFAULT_PKCS11_LIB
#define AOS_CONFIG_CRYPTO_DEFAULT_PKCS11_LIB "/usr/lib/softhsm/libsofthsm2.so"
#endif

/**
 * Use PKCS11 OS locking mechanism.
 */
#ifndef AOS_CONFIG_CRYPTO_PKCS11_OS_LOCKING
#define AOS_CONFIG_CRYPTO_PKCS11_OS_LOCKING 1
#endif

/**
 * Maximum number of public keys to be allocated by cryptoprovider simultaneously.
 */
#ifndef AOS_CONFIG_CRYPTO_PUB_KEYS_COUNT
#define AOS_CONFIG_CRYPTO_PUB_KEYS_COUNT 8
#endif

/**
 * Maximum number of hasher instances to be allocated by cryptoprovider.
 */
#ifndef AOS_CONFIG_CRYPTO_HASHER_COUNT
#define AOS_CONFIG_CRYPTO_HASHER_COUNT 32
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
#define AOS_CONFIG_PKCS11_PIN_LEN 50
#endif

/**
 * Length of randomly generated PIN.
 */
#ifndef AOS_CONFIG_PKCS11_GEN_PIN_LEN
#define AOS_CONFIG_PKCS11_GEN_PIN_LEN 30
#endif

/**
 * Maximum size of PKCS11 ID.
 */
#ifndef AOS_CONFIG_PKCS11_ID_SIZE
#define AOS_CONFIG_PKCS11_ID_SIZE 32
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
 * Maximum size of session pool in PKCS11 LibraryContext.
 */
#ifndef AOS_CONFIG_PKCS11_SESSION_POOL_MAX_SIZE
#define AOS_CONFIG_PKCS11_SESSION_POOL_MAX_SIZE 2
#endif

/**
 * Max media type len.
 */
#ifndef AOS_CONFIG_OCISPEC_MEDIA_TYPE_LEN
#define AOS_CONFIG_OCISPEC_MEDIA_TYPE_LEN 64
#endif

/**
 * Max spec parameter len.
 */
#ifndef AOS_CONFIG_OCISPEC_MAX_SPEC_PARAM_LEN
#define AOS_CONFIG_OCISPEC_MAX_SPEC_PARAM_LEN 256
#endif

/**
 * Spec parameter max count.
 */
#ifndef AOS_CONFIG_OCISPEC_MAX_SPEC_PARAM_COUNT
#define AOS_CONFIG_OCISPEC_MAX_SPEC_PARAM_COUNT 16
#endif

/**
 * Max device type len.
 */
#ifndef AOS_CONFIG_OCISPEC_DEV_TYPE_LEN
#define AOS_CONFIG_OCISPEC_DEV_TYPE_LEN 32
#endif

/**
 * Max DT devices count.
 */
#ifndef AOS_CONFIG_OCISPEC_MAX_DT_DEVICES_COUNT
#define AOS_CONFIG_OCISPEC_MAX_DT_DEVICES_COUNT 20
#endif

/**
 * Max DT device name length.
 */
#ifndef AOS_CONFIG_OCISPEC_DT_DEV_NAME_LEN
#define AOS_CONFIG_OCISPEC_DT_DEV_NAME_LEN 32
#endif

/**
 * Max IOMEMs count.
 */
#ifndef AOS_CONFIG_OCISPEC_MAX_IOMEMS_COUNT
#define AOS_CONFIG_OCISPEC_MAX_IOMEMS_COUNT 20
#endif

/**
 * Max IRQs count.
 */
#ifndef AOS_CONFIG_OCISPEC_MAX_IRQS_COUNT
#define AOS_CONFIG_OCISPEC_MAX_IRQS_COUNT 20
#endif

/**
 * Author len.
 */
#ifndef AOS_CONFIG_OCISPEC_AUTHOR_LEN
#define AOS_CONFIG_OCISPEC_AUTHOR_LEN 64
#endif

/**
 * Balancing policy len.
 */
#ifndef AOS_CONFIG_OCISPEC_BALANCING_POLICY_LEN
#define AOS_CONFIG_OCISPEC_BALANCING_POLICY_LEN 32
#endif

/**
 * Max sysctl name len.
 */
#ifndef AOS_CONFIG_OCISPEC_SYSCTL_LEN
#define AOS_CONFIG_OCISPEC_SYSCTL_LEN 32
#endif

/**
 * Max sysctl count.
 */
#ifndef AOS_CONFIG_OCISPEC_SYSCTL_MAX_COUNT
#define AOS_CONFIG_OCISPEC_SYSCTL_MAX_COUNT 32
#endif

/**
 * User name len.
 */
#ifndef AOS_CONFIG_OCISPEC_USER_NAME_LEN
#define AOS_CONFIG_OCISPEC_USER_NAME_LEN 64
#endif

/**
 * Alert message len.
 */
#ifndef AOS_CONFIG_CLOUDPROTOCOL_ALERT_MESSAGE_LEN
#define AOS_CONFIG_CLOUDPROTOCOL_ALERT_MESSAGE_LEN 128
#endif

/**
 * Alert download target id len.
 */
#ifndef AOS_CONFIG_CLOUDPROTOCOL_ALERT_CORE_DOWNLOAD_TARGET_ID_LEN
#define AOS_CONFIG_CLOUDPROTOCOL_ALERT_CORE_DOWNLOAD_TARGET_ID_LEN 64
#endif

/**
 * Alert download progress len.
 */
#ifndef AOS_CONFIG_CLOUDPROTOCOL_ALERT_DOWNLOAD_PROGRESS_LEN
#define AOS_CONFIG_CLOUDPROTOCOL_ALERT_DOWNLOAD_PROGRESS_LEN 64
#endif

/**
 * Alert parameter len.
 */
#ifndef AOS_CONFIG_CLOUDPROTOCOL_ALERT_PARAMETER_LEN
#define AOS_CONFIG_CLOUDPROTOCOL_ALERT_PARAMETER_LEN 16
#endif

/**
 * Resource alert errors size.
 */
#ifndef AOS_CONFIG_CLOUDPROTOCOL_ALERT_RESOURCE_ERRORS_SIZE
#define AOS_CONFIG_CLOUDPROTOCOL_ALERT_RESOURCE_ERRORS_SIZE 4
#endif

/**
 * Environment variable name value.
 */
#ifndef AOS_CONFIG_CLOUDPROTOCOL_ENV_VAR_VALUE_LEN
#define AOS_CONFIG_CLOUDPROTOCOL_ENV_VAR_VALUE_LEN 32
#endif

/**
 * Log id len.
 */
#ifndef AOS_CONFIG_CLOUDPROTOCOL_LOG_ID_LEN
#define AOS_CONFIG_CLOUDPROTOCOL_LOG_ID_LEN 64
#endif

/**
 * Log content len.
 */
#ifndef AOS_CONFIG_CLOUDPROTOCOL_LOG_CONTENT_LEN
#define AOS_CONFIG_CLOUDPROTOCOL_LOG_CONTENT_LEN 256 * 1024
#endif

/**
 * Bearer token len.
 */
#ifndef AOS_CONFIG_CLOUDPROTOCOL_BEARER_TOKEN_LEN
#define AOS_CONFIG_CLOUDPROTOCOL_BEARER_TOKEN_LEN 256
#endif

#endif
