/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_LOG_HPP_
#define CONFIG_LOG_HPP_

/**
 * Log levels.
 *
 */
#define CONFIG_LOG_LEVEL_DISABLE 0
#define CONFIG_LOG_LEVEL_ERROR   1
#define CONFIG_LOG_LEVEL_WARNING 2
#define CONFIG_LOG_LEVEL_INFO    3
#define CONFIG_LOG_LEVEL_DEBUG   4

/**
 * Configures log level.
 *
 */
#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL CONFIG_LOG_LEVEL_DEBUG
#endif

/**
 * Max log line size.
 *
 */
#ifndef CONFIG_LOG_LINE_SIZE
#define CONFIG_LOG_LINE_SIZE 120
#endif

#endif
