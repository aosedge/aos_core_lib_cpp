/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_COMMON_CONFIG_HPP_
#define AOS_COMMON_CONFIG_HPP_

/**
 * Service/layer description len.
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

#endif
