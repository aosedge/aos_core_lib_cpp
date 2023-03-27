/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SM_CONFIG_HPP_
#define AOS_SM_CONFIG_HPP_

/**
 * Number of parallel instance launches.
 */
#ifndef AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES
#define AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES 5
#endif

/**
 * Aos runtime dir.
 */
#ifndef AOS_CONFIG_LAUNCHER_RUNTIME_DIR
#define AOS_CONFIG_LAUNCHER_RUNTIME_DIR "/run/aos/runtime"
#endif

/**
 * Number of parallel service installs.
 */
#ifndef AOS_CONFIG_SERVICEMANAGER_NUM_COOPERATE_INSTALLS
#define AOS_CONFIG_SERVICEMANAGER_NUM_COOPERATE_INSTALLS 5
#endif

#endif
