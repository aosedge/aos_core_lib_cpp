/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_UTILS_HPP_
#define AOS_UTILS_HPP_

#include <cstddef>

namespace aos {

/**
 * Defines array size.
 */
template <typename T, size_t n>
constexpr size_t ArraySize(T (&)[n])
{
    return n;
};

/**
 * Calculates aligned by machine word size.
 *
 * @param size input size.
 * @return constexpr size_t aligned size.
 */
constexpr size_t AlignedSize(size_t size)
{
    return (size + sizeof(int) - 1) / sizeof(int) * sizeof(int);
};

/**
 * Implements struct of pair fields.
 * @tparam F
 * @tparam S
 */
template <typename F, typename S>
struct Pair {
    Pair(F f, S s)
        : mFirst(f)
        , mSecond(s) {};

    F mFirst;
    S mSecond;
};

} // namespace aos

#endif
