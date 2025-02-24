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
 * Launcher thread task size.
 */
#ifndef AOS_CONFIG_LAUNCHER_THREAD_TASK_SIZE
#define AOS_CONFIG_LAUNCHER_THREAD_TASK_SIZE 512
#endif

/**
 * Launcher thread stack size.
 */
#ifndef AOS_CONFIG_LAUNCHER_THREAD_STACK_SIZE
#define AOS_CONFIG_LAUNCHER_THREAD_STACK_SIZE 0
#endif

/**
 * Aos runtime dir.
 */
#ifndef AOS_CONFIG_LAUNCHER_RUNTIME_DIR
#define AOS_CONFIG_LAUNCHER_RUNTIME_DIR "/run/aos/runtime"
#endif

/**
 * Max num of host directory binds.
 */
#ifndef AOS_CONFIG_LAUNCHER_MAX_NUM_HOST_BINDS
#define AOS_CONFIG_LAUNCHER_MAX_NUM_HOST_BINDS 64
#endif

/**
 * Enable cgroup v2.
 */
#ifndef AOS_CONFIG_LAUNCHER_CGROUP_V2
#define AOS_CONFIG_LAUNCHER_CGROUP_V2 1
#endif

/**
 * Number of parallel service installs.
 */
#ifndef AOS_CONFIG_SERVICEMANAGER_NUM_COOPERATE_INSTALLS
#define AOS_CONFIG_SERVICEMANAGER_NUM_COOPERATE_INSTALLS 5
#endif

/**
 * Node config JSON length.
 */
#ifndef AOS_CONFIG_RESOURCEMANAGER_NODE_CONFIG_JSON_LEN
#define AOS_CONFIG_RESOURCEMANAGER_NODE_CONFIG_JSON_LEN 4096
#endif

/**
 * Max number of network manager aliases.
 */
#ifndef AOS_CONFIG_NETWORKMANAGER_MAX_NUM_ALIASES
#define AOS_CONFIG_NETWORKMANAGER_MAX_NUM_ALIASES 4
#endif

/**
 * Max number of network manager exposed ports.
 */
#ifndef AOS_CONFIG_NETWORKMANAGER_MAX_NUM_EXPOSED_PORTS
#define AOS_CONFIG_NETWORKMANAGER_MAX_NUM_EXPOSED_PORTS 8
#endif

/**
 * Max resolv.conf line length.
 */
#ifndef AOS_CONFIG_NETWORKMANAGER_RESOLV_CONF_LINE_LEN
#define AOS_CONFIG_NETWORKMANAGER_RESOLV_CONF_LINE_LEN 256
#endif

#ifndef AOS_CONFIG_NETWORKMANAGER_MAX_NUM_HOSTS
#define AOS_CONFIG_NETWORKMANAGER_MAX_NUM_HOSTS 10
#endif

/**
 * Max CNI name length.
 */
#ifndef AOS_CONFIG_CNI_NAME_LEN
#define AOS_CONFIG_CNI_NAME_LEN 64
#endif

/**
 * Max CNI version length.
 */
#ifndef AOS_CONFIG_CNI_VERSION_LEN
#define AOS_CONFIG_CNI_VERSION_LEN 10
#endif

/**
 * Max CNI plugin type length.
 */
#ifndef AOS_CONFIG_CNI_PLUGIN_TYPE_LEN
#define AOS_CONFIG_CNI_PLUGIN_TYPE_LEN 64
#endif

/**
 * Max CNI number of routers.
 */
#ifndef AOS_CONFIG_CNI_MAX_NUM_ROUTERS
#define AOS_CONFIG_CNI_MAX_NUM_ROUTERS 4
#endif

/**
 * Max CNI number of interfaces.
 */
#ifndef AOS_CONFIG_CNI_MAX_NUM_INTERFACES
#define AOS_CONFIG_CNI_MAX_NUM_INTERFACES 6
#endif

/**
 * Max CNI number of IPs.
 */
#ifndef AOS_CONFIG_CNI_MAX_NUM_IPS
#define AOS_CONFIG_CNI_MAX_NUM_IPS 4
#endif

/**
 * Max CNI number of runtime config arguments.
 */
#ifndef AOS_CONFIG_CNI_MAX_NUM_RUNTIME_CONFIG_ARGS
#define AOS_CONFIG_CNI_MAX_NUM_RUNTIME_CONFIG_ARGS 4
#endif

/**
 * Max CNI runtime config argument length.
 */
#ifndef AOS_CONFIG_CNI_RUNTIME_CONFIG_ARG_LEN
#define AOS_CONFIG_CNI_RUNTIME_CONFIG_ARG_LEN 64
#endif

/**
 * Max CNI plugin name length.
 */
#ifndef AOS_CONFIG_CNI_PLUGIN_NAME_LEN
#define AOS_CONFIG_CNI_PLUGIN_NAME_LEN 64
#endif

#endif
