/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_TESTCONFIG_HPP_
#define AOS_CORE_TESTCONFIG_HPP_

/**
 * URL length.
 */
#define AOS_CONFIG_TYPES_URL_LEN 512

/**
 * File/directory path len.
 */
#define AOS_CONFIG_TYPES_FILE_PATH_LEN 512

/**
 * Max number of services.
 */
#define AOS_CONFIG_TYPES_MAX_NUM_SERVICES 8

/**
 * Max number of layers.
 */
#define AOS_CONFIG_TYPES_MAX_NUM_LAYERS 8

/**
 * Monitoring poll period.
 */
#define AOS_CONFIG_MONITORING_POLL_PERIOD_SEC 1

/**
 * Monitoring average window.
 */
#define AOS_CONFIG_MONITORING_AVERAGE_WINDOW_SEC 3

/**
 * Aos runtime dir.
 */
#define AOS_CONFIG_LAUNCHER_RUNTIME_DIR "/tmp/aos/runtime"

#endif // AOS_CORE_TESTCONFIG_HPP_
