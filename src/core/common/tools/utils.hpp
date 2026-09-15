/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TOOLS_UTILS_HPP_
#define AOS_CORE_COMMON_TOOLS_UTILS_HPP_

#include <cstddef>
#include <cstdint>

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
constexpr size_t AlignedSize(size_t size, size_t align = sizeof(int32_t))
{
    return (size + align - 1) / align * align;
};

/**
 * Remove reference template.
 *
 * @tparam T reference type.
 */
template <typename T>
struct RemoveRef {
    using type = T;
};

/**
 * Remove reference template.
 *
 * @tparam T reference type.
 */
template <typename T>
struct RemoveRef<T&> {
    using type = T;
};

/**
 * Remove reference template.
 *
 * @tparam T reference type.
 */
template <typename T>
struct RemoveRef<T&&> {
    using type = T;
};

/**
 * Remove const template.
 *
 * @tparam T const type.
 */
template <typename T>
struct RemoveConst {
    using type = T;
};

/**
 * Remove const template.
 *
 * @tparam T const type.
 */
template <typename T>
struct RemoveConst<const T> {
    using type = T;
};

template <class T>
using RemoveConstType = typename RemoveConst<T>::type;

/**
 * Move template.
 *
 * @tparam T object to move.
 */
template <typename T>
inline typename RemoveRef<T>::type&& Move(T&& object) // NOSONAR cpp:M23_279 - this function itself implements the
                                                      // move idiom, it must not forward its argument
{
    return static_cast<typename RemoveRef<T>::type&&>(object);
}

/**
 * Forward template. Preserves the value category of a forwarding reference argument when
 * passing it on to another function.
 *
 * @tparam T object to forward.
 */
template <typename T>
inline T&& Forward(typename RemoveRef<T>::type& object)
{
    return static_cast<T&&>(object);
}

/**
 * Forward template. Preserves the value category of a forwarding reference argument when
 * passing it on to another function.
 *
 * @tparam T object to forward.
 */
template <typename T>
inline T&& Forward(typename RemoveRef<T>::type&& object)
{
    return static_cast<T&&>(object);
}

/**
 * Implements struct of pair fields.
 * @tparam F
 * @tparam S
 */
template <typename F, typename S>
struct Pair {
    /**
     * Default constructor.
     */
    Pair()
        : mFirst()
        , mSecond()
    {
    }

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
     * Constructor.
     *
     * @param @f first value.
     * @param @args arguments to create a second parameter.
     */
    template <typename... Args>
    Pair(const F& f, Args&&... args) // NOSONAR cpp:S1709 - implicit conversion from the underlying/raw type is
                                     // intentional, core to this type's value-semantics ergonomics
        : mFirst(f)
        , mSecond(Forward<Args>(args)...)
    {
    }

    /**
     * Comparison operators.
     */
    friend bool operator==(const Pair& lhs, const Pair<F, S>& other)
    {
        return lhs.mFirst == other.mFirst && lhs.mSecond == other.mSecond;
    };
    friend bool operator!=(const Pair& lhs, const Pair<F, S>& other) { return !(lhs == other); };

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
 * Forward conditional template.
 *
 * @tparam B condition.
 * @tparam T true type.
 * @tparam F false type.
 */
template <bool B, typename T, typename F>
struct ConditionalStruct {
    using type = T;
};

/**
 * Forward conditional template.
 *
 * @tparam T true type.
 * @tparam F false type.
 */
template <typename T, typename F>
struct ConditionalStruct<false, T, F> {
    using type = F;
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
    using type = T;
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
    using Yes = char[1];
    using No  = char[2];

    static Yes& Test(B*);
    static No&  Test(...);

public:
    static const bool value = sizeof(Test(static_cast<D*>(nullptr))) == sizeof(Yes);
};

/**
 * Integral constant template.
 *
 * @tparam T type.
 * @tparam v value.
 */
template <typename T, auto v>
struct IntegralConstant {
    static constexpr T value = v;
    using ValueType          = T;
    using Type               = IntegralConstant;
    constexpr operator ValueType() const noexcept // NOSONAR cpp:S1709
    {
        return value;
    }
};

/**
 * True type template.
 */
using TrueType = IntegralConstant<bool, true>;

/**
 * False type template.
 */
using FalseType = IntegralConstant<bool, false>;

/**
 * Declval template.
 *
 * @tparam T type.
 */
template <typename T>
T&& declval() noexcept; // no definition needed

/**
 * Make void template.
 *
 * @tparam ...T types.
 */
template <typename...>
struct make_void {
    using type = void;
};

/**
 * Primary template: false unless conversion is possible.
 * @tparam From source type
 * @tparam To   target type
 */
template <typename From, typename To, typename = void>
struct IsConvertible : FalseType { };

/**
 * Specialization: true if static_cast<To>(declval<From>()) is valid.
 * @tparam From source type
 * @tparam To   target type
 */
template <typename From, typename To>
struct IsConvertible<From, To, typename make_void<decltype(static_cast<To>(declval<From>()))>::type> : TrueType { };

/**
 * constexpr bool alias for is_convertible<From,To>::value.
 * @tparam From source type
 * @tparam To   target type
 */
template <typename From, typename To>
constexpr bool IsConvertible_v = IsConvertible<From, To>::value;

} // namespace aos

#endif
