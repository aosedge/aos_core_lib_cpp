/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CONFIG_THREAD_HPP_
#define AOS_CONFIG_THREAD_HPP_

/**
 * Configures default thread stack size.
 *
 * Use minimal stack size PTHREAD_STACK_MIN + 2k for storing thread functor.
 */
#ifndef AOS_CONFIG_THREAD_DEFAULT_STACK_SIZE
#define AOS_CONFIG_THREAD_DEFAULT_STACK_SIZE (16384 + 2048)
#endif

#endif
