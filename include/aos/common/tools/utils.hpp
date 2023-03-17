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
template <typename T, size_t cSize>
constexpr size_t ArraySize(T (&)[cSize])
{
    return cSize;
};

/**
 * Calculates aligned by machine word size.
 *
 * @param size input size.
 * @param align alignment.
 * @return constexpr size_t aligned size.
 */
constexpr size_t AlignedSize(size_t size, size_t align = sizeof(int))
{
    return (size + align - 1) / align * align;
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

/**
 * Returns min from two value.
 */
template <typename T>
constexpr T Min(T a, T b)
{
    return (a < b) ? a : b;
};

/**
 * Returns max from two value.
 */
template <typename T>
constexpr T Max(T a, T b)
{
    return (a > b) ? a : b;
};

/**
 * Returns min value.
 */
template <typename T, typename... Args>
constexpr T Min(T value, Args... args)
{
    return Min(value, Min(args...));
};

/**
 * Returns max value.
 */
template <typename T, typename... Args>
constexpr T Max(T value, Args... args)
{
    return Max(value, Max(args...));
}

} // namespace aos

#endif
