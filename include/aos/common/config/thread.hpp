/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CONFIG_THREAD_HPP_
#define AOS_CONFIG_THREAD_HPP_

/**
 * Configures function max size.
 */
#ifndef AOS_CONFIG_FUNCTION_MAX_SIZE
#define AOS_CONFIG_FUNCTION_MAX_SIZE 256
#endif

/**
 * Configures max thread task size.
 */
#ifndef AOS_CONFIG_THREAD_DEFAULT_MAX_FUNCTION_SIZE
#define AOS_CONFIG_THREAD_DEFAULT_MAX_FUNCTION_SIZE 256
#endif

/**
 * Configures default thread stack size.
 *
 * Use minimal stack size PTHREAD_STACK_MIN + 2k for storing thread functor.
 */
#ifndef AOS_CONFIG_THREAD_DEFAULT_STACK_SIZE
#define AOS_CONFIG_THREAD_DEFAULT_STACK_SIZE (16384 + 2048)
#endif

/**
 * Configures thread stack alignment.
 */
#ifndef AOS_CONFIG_THREAD_STACK_ALIGN
#define AOS_CONFIG_THREAD_STACK_ALIGN sizeof(int)
#endif

/**
 * Configures default thread pool queue size.
 */
#ifndef AOS_CONFIG_THREAD_POOL_DEFAULT_QUEUE_SIZE
#define AOS_CONFIG_THREAD_POOL_DEFAULT_QUEUE_SIZE 1024
#endif

#endif
