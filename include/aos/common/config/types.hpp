/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CONFIG_TYPES_HPP_
#define AOS_CONFIG_TYPES_HPP_

/**
 * Service ID len.
 */
#ifndef CONFIG_SERVICE_ID_LEN
#define CONFIG_SERVICE_ID_LEN 40
#endif

/**
 * Subject ID len.
 */
#ifndef CONFIG_SUBJECT_ID_LEN
#define CONFIG_SUBJECT_ID_LEN 40
#endif

/**
 * File/directory path len.
 */
#ifndef CONFIG_FILE_PATH_LEN
#define CONFIG_FILE_PATH_LEN 256
#endif

/**
 * Instance state checksum len.
 */
#ifndef CONFIG_STATE_CHECKSUM_LEN
#define CONFIG_STATE_CHECKSUM_LEN 28
#endif

/**
 * Instance ID len.
 */
#ifndef CONFIG_INSTANCE_ID_LEN
#define CONFIG_INSTANCE_ID_LEN 40
#endif

#endif
