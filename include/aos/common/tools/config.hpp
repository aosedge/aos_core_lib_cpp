/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_TOOLS_CONFIG_HPP_
#define AOS_TOOLS_CONFIG_HPP_

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
#define AOS_CONFIG_LOG_LINE_LEN 256
#endif

/**
 * Configures function max size.
 */
#ifndef AOS_CONFIG_FUNCTION_MAX_SIZE
#define AOS_CONFIG_FUNCTION_MAX_SIZE 64
#endif

/**
 * Configures default thread stack size.
 *
 * Use minimal stack size PTHREAD_STACK_MIN.
 */
#ifndef AOS_CONFIG_THREAD_DEFAULT_STACK_SIZE
#define AOS_CONFIG_THREAD_DEFAULT_STACK_SIZE 16384
#endif

/**
 * Configures thread stack alignment.
 */
#ifndef AOS_CONFIG_THREAD_STACK_ALIGN
#define AOS_CONFIG_THREAD_STACK_ALIGN 4096
#endif

/**
 * Configures thread stack guard size.
 */
#ifndef AOS_CONFIG_THREAD_STACK_GUARD_SIZE
#define AOS_CONFIG_THREAD_STACK_GUARD_SIZE 0
#endif

/**
 * Configures default thread pool queue size.
 */
#ifndef AOS_CONFIG_THREAD_POOL_DEFAULT_QUEUE_SIZE
#define AOS_CONFIG_THREAD_POOL_DEFAULT_QUEUE_SIZE 16
#endif

/**
 * Configures clock ID used for thread time operations.
 */
#ifndef AOS_CONFIG_THREAD_CLOCK_ID
#define AOS_CONFIG_THREAD_CLOCK_ID CLOCK_REALTIME
#endif

/**
 * Enables Aos custom new operators.
 */
#ifndef AOS_CONFIG_NEW_USE_AOS
#define AOS_CONFIG_NEW_USE_AOS 0
#endif

/**
 * Timer signal event notification.
 */
#ifndef AOS_CONFIG_TIMER_SIGEV_NOTIFY
#define AOS_CONFIG_TIMER_SIGEV_NOTIFY SIGEV_THREAD
#endif

/**
 * File/directory path len.
 */
#ifndef AOS_CONFIG_FS_FILE_PATH_LEN
#define AOS_CONFIG_FS_FILE_PATH_LEN 256
#endif

/**
 * Size of a time in string representation.
 */
#ifndef AOS_CONFIG_TIME_STR_LEN
#define AOS_CONFIG_TIME_STR_LEN 68
#endif

#endif
