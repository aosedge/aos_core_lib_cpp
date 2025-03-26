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
    /**
     * Constructor.
     *
     * @param @f first value
     * @param @s second value
     */
    Pair(const F& f, const S& s)
        : mFirst(f)
        , mSecond(s) {};

    /**
     * Comparison operators.
     */
    bool operator==(const Pair<F, S>& other) const { return mFirst == other.mFirst && mSecond == other.mSecond; }
    bool operator!=(const Pair<F, S>& other) const { return !(*this == other); }

    /**
     * Pair first value.
     */
    F mFirst;

    /**
     * Pair second value.
     */
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

/**
 * Remove reference template.
 *
 * @tparam T reference type.
 */
template <typename T>
struct RemoveRef {
    typedef T type;
};

/**
 * Remove reference template.
 *
 * @tparam T reference type.
 */
template <typename T>
struct RemoveRef<T&> {
    typedef T type;
};

/**
 * Remove reference template.
 *
 * @tparam T reference type.
 */
template <typename T>
struct RemoveRef<T&&> {
    typedef T type;
};

/**
 * Remove const template.
 *
 * @tparam T const type.
 */
template <typename T>
struct RemoveConst {
    typedef T type;
};

/**
 * Remove const template.
 *
 * @tparam T const type.
 */
template <typename T>
struct RemoveConst<const T> {
    typedef T type;
};

template <class T>
using RemoveConstType = typename RemoveConst<T>::type;

/**
 * Move template.
 *
 * @tparam T object to move.
 */
template <typename T>
inline typename RemoveRef<T>::type&& Move(T&& object)
{
    return static_cast<typename RemoveRef<T>::type&&>(object);
}

/**
 * Forward conditional template.
 *
 * @tparam B condition.
 * @tparam T true type.
 * @tparam F false type.
 */
template <bool B, typename T, typename F>
struct ConditionalStruct {
    typedef T type;
};

/**
 * Forward conditional template.
 *
 * @tparam T true type.
 * @tparam F false type.
 */
template <typename T, typename F>
struct ConditionalStruct<false, T, F> {
    typedef F type;
};

/**
 * Conditional template.
 *
 * @tparam B condition.
 * @tparam T true type.
 * @tparam F false type.
 */
template <bool B, typename T, typename F>
using Conditional = typename ConditionalStruct<B, T, F>::type;

/**
 * Forward condition template.
 *
 * @tparam B condition.
 * @tparam T true type.
 */
template <bool B, typename T = void>
struct EnableStruct { };

/**
 * Forward condition template.
 *
 * @tparam T
 */
template <typename T>
struct EnableStruct<true, T> {
    typedef T type;
};

/**
 * Enable if template.
 *
 * @tparam B condition.
 * @tparam T true type.
 */
template <bool B, typename T = void>
using EnableIf = typename EnableStruct<B, T>::type;

/**
 * Checks if a type is a base of the other type.
 *
 * @tparam B base type.
 * @tparam D derived type.
 */
template <typename B, typename D>
struct IsBaseOf {
private:
    typedef char Yes[1];
    typedef char No[2];

    static Yes& Test(B*);
    static No&  Test(...);

public:
    static const bool value = sizeof(Test(static_cast<D*>(0))) == sizeof(Yes);
};

} // namespace aos

#endif
