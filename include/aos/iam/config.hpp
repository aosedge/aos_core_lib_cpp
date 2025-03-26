/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IAM_CONFIG_HPP_
#define AOS_IAM_CONFIG_HPP_

/**
 * Max length of certificate type.
 */
#ifndef AOS_CONFIG_CERTHANDLER_CERT_TYPE_NAME_LEN
#define AOS_CONFIG_CERTHANDLER_CERT_TYPE_NAME_LEN 16
#endif

/**
 * Max expected number of certificates per IAM certificate module.
 */
#ifndef AOS_CONFIG_CERTHANDLER_CERTS_PER_MODULE
#define AOS_CONFIG_CERTHANDLER_CERTS_PER_MODULE 5
#endif

/**
 * Password max length.
 */
#ifndef AOS_CONFIG_CERTHANDLER_PASSWORD_LEN
#define AOS_CONFIG_CERTHANDLER_PASSWORD_LEN 40
#endif

/**
 * Max number of key usages for a module.
 */
#ifndef AOS_CONFIG_CERTHANDLER_KEY_USAGE_MAX_COUNT
#define AOS_CONFIG_CERTHANDLER_KEY_USAGE_MAX_COUNT 2
#endif

/**
 * Maximum length of distinguished name string representation.
 */
#ifndef AOS_CONFIG_CERTHANDLER_DN_STRING_LEN
#define AOS_CONFIG_CERTHANDLER_DN_STRING_LEN 128
#endif

/**
 * Max number of certificate modules.
 */
#ifndef AOS_CONFIG_CERTHANDLER_MODULES_MAX_COUNT
#define AOS_CONFIG_CERTHANDLER_MODULES_MAX_COUNT 8
#endif

/**
 * Max number of certificate change subscriptions.
 */
#ifndef AOS_CONFIG_CERTHANDLER_CERT_SUBS_MAX_COUNT
#define AOS_CONFIG_CERTHANDLER_CERT_SUBS_MAX_COUNT AOS_CONFIG_CERTHANDLER_MODULES_MAX_COUNT * 3
#endif

/**
 * Maximum length of TEE Login type.
 */
#ifndef AOS_CONFIG_CERTMODULE_PKCS11_TEE_LOGIN_TYPE_LEN
#define AOS_CONFIG_CERTMODULE_PKCS11_TEE_LOGIN_TYPE_LEN 8
#endif

/**
 * Maximum length of permhandler secret.
 */
#ifndef AOS_CONFIG_PERMHANDLER_SECRET_LEN
#define AOS_CONFIG_PERMHANDLER_SECRET_LEN 42
#endif

/**
 * Maximum number of nodes handled by node manager.
 */
#ifndef AOS_CONFIG_NODEMANAGER_NODE_MAX_NUM
#define AOS_CONFIG_NODEMANAGER_NODE_MAX_NUM 4
#endif

#endif
