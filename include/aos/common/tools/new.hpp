/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_NEW_HPP_
#define AOS_NEW_HPP_

#include <new>
#include <stddef.h>

#include "aos/common/config/new.hpp"

#if AOS_CONFIG_NEW_USE_AOS

inline void* operator new(size_t, void* data)
{
    return data;
}

#endif

#endif
