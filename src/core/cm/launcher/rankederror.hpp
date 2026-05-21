/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_RANKEDERROR_HPP_
#define AOS_CORE_CM_LAUNCHER_RANKEDERROR_HPP_

#include <core/common/tools/error.hpp>

namespace aos::cm::launcher {

/**
 * Error container with explicit error rank.
 */
struct RankedError {
    static constexpr int cLowErrorRank  = 1;
    static constexpr int cHighErrorRank = 2;
    static constexpr int cNoErrorRank   = 3;

    // cppcheck-suppress noExplicitConstructor
    RankedError(const Error& error = ErrorEnum::eNone, int errorRank = cNoErrorRank)
        : mError(error)
        , mErrorRank(errorRank)
    {
    }

    bool IsNone() const { return mError.IsNone(); }

    /**
     * Selects error with higher rank.
     *
     * If one of errors is none, returns the other one.
     */
    static RankedError SelectHigherRankedError(const RankedError& left, const RankedError& right)
    {
        if (left.IsNone()) {
            return right;
        }

        if (right.IsNone()) {
            return left;
        }

        return left.mErrorRank >= right.mErrorRank ? left : right;
    }

    Error mError;
    int   mErrorRank {};
};

/**
 * Container that holds value and ranked error.
 *
 * @tparam T value type.
 */
template <typename T>
struct RetWithRankedError {
    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs return value with error instance and rank.
     *
     * @param value return value.
     * @param error return error.
     * @param errorRank return error rank.
     */
    RetWithRankedError(const T& value, const Error& error = ErrorEnum::eNone, int errorRank = RankedError::cNoErrorRank)
        : mValue(value)
        , mRankedError(error, errorRank)
    {
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs return value with error instance and rank.
     *
     * @param value return value.
     * @param error return error.
     * @param errorRank return error rank.
     */
    RetWithRankedError(T&& value, const Error& error = ErrorEnum::eNone, int errorRank = RankedError::cNoErrorRank)
        : mValue(Move(value))
        , mRankedError(error, errorRank)
    {
    }

    /**
     * Comparison operators.
     */
    bool operator==(const RetWithRankedError<T>& other) const
    {
        return mValue == other.mValue && mRankedError.mError == other.mRankedError.mError
            && mRankedError.mErrorRank == other.mRankedError.mErrorRank;
    }
    bool operator!=(const RetWithRankedError<T>& other) const { return !(*this == other); }

    /**
     * Holds returned value.
     */
    T mValue;

    /**
     * Holds returned ranked error.
     */
    RankedError mRankedError;
};

} // namespace aos::cm::launcher

#endif
