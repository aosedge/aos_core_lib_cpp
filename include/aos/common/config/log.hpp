/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CONFIG_LOG_HPP_
#define AOS_CONFIG_LOG_HPP_

/**
 * Log levels.
 *
 */
#define AOS_CONFIG_LOG_LEVEL_DISABLE 0
#define AOS_CONFIG_LOG_LEVEL_ERROR   1
#define AOS_CONFIG_LOG_LEVEL_WARNING 2
#define AOS_CONFIG_LOG_LEVEL_INFO    3
#define AOS_CONFIG_LOG_LEVEL_DEBUG   4

/**
 * Configures log level.
 *
 */
#ifndef AOS_CONFIG_LOG_LEVEL
#define AOS_CONFIG_LOG_LEVEL AOS_CONFIG_LOG_LEVEL_DEBUG
#endif

/**
 * Max log line size.
 *
 */
#ifndef AOS_CONFIG_LOG_LINE_LEN
#define AOS_CONFIG_LOG_LINE_LEN 120
#endif

#endif
