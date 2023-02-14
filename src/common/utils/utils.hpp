/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTILS_HPP_
#define UTILS_HPP_

#include <cstddef>

/**
 * Defines array size.
 */
template <typename T, size_t n>
constexpr size_t ArraySize(T (&)[n])
{
    return n;
}

/**
 * Implements struct of pair fields.
 * @tparam F
 * @tparam S
 */
template <typename F, typename S>
struct Pair {
    Pair(F f, S s)
        : first(f)
        , second(s) {};

    F first;
    S second;
};

#endif
