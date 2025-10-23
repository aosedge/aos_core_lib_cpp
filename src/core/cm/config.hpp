/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_CONFIG_HPP_
#define AOS_CORE_CM_CONFIG_HPP_

/**
 * Number of parallel update items actions.
 */
#ifndef AOS_CONFIG_IMAGEMANAGER_NUM_COOPERATE_ACTIONS
#define AOS_CONFIG_IMAGEMANAGER_NUM_COOPERATE_ACTIONS 5
#endif

/**
 * Service discovery supported protocols count.
 */
#ifndef AOS_CONFIG_CM_SERVICE_DISCOVERY_PROTOCOLS_COUNT
#define AOS_CONFIG_CM_SERVICE_DISCOVERY_PROTOCOLS_COUNT 1
#endif

/**
 * Alerts cache size.
 */
#ifndef AOS_CONFIG_CM_ALERTS_CACHE_SIZE
#define AOS_CONFIG_CM_ALERTS_CACHE_SIZE 32
#endif

#endif
