/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ERROR_HPP_
#define AOS_ERROR_HPP_

#include <errno.h>
#include <string.h>

#include "aos/common/stringer.hpp"

namespace aos {

/**
 * Error types.
 *
 */
class ErrorType {
public:
    enum class Enum { eNone, eFailed, eRuntime, eNoMemory, eOutOfRange, eNotFound, eInvalidArgument, eNumErrors };

    static Pair<const char* const*, size_t> GetStrings()
    {
        static const char* const cErrorTypeStrings[static_cast<size_t>(Enum::eNumErrors)]
            = {"none", "failed", "runtime error", "not enough memory", "out of range", "invalid argument", "not found"};

        return Pair<const char* const*, size_t>(cErrorTypeStrings, static_cast<size_t>(Enum::eNumErrors));
    };
};

using ErrorEnum = ErrorType::Enum;

/**
 * Aos errors.
 */
class Error : public EnumStringer<ErrorType> {
public:
    /**
     * Constructs default error instance.
     */
    Error()
        : mErrno(0)
    {
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs error instance from ErrorType::Enum.
     */
    Error(ErrorEnum err)
        : EnumStringer(err)
        , mErrno(0)
    {
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs error instance from system errno value.
     *
     * @param e errno.
     */
    Error(int errNo)
        : EnumStringer(errNo == 0 ? ErrorEnum::eNone : ErrorEnum::eRuntime)
        , mErrno(errNo)
    {
    }

    /**
     * Checks if error is none.
     */
    bool IsNone() const { return GetValue() == ErrorEnum::eNone; }

    /**
     * Checks if error has specified type.
     *
     * @param e error to check.
     * @return bool result.
     */
    bool Is(const Error& e) const
    {
        if (mErrno != 0) {
            return mErrno == e.mErrno;
        }

        return GetValue() == e.GetValue();
    }

    /**
     * Implements own ToString method.
     *
     * @return const char* string.
     */
    const char* ToString() const override
    {
        if (mErrno != 0) {
            return strerror(mErrno);
        }

        return EnumStringer<ErrorType>::ToString();
    }

private:
    int mErrno;
};

template <typename T>
struct RetWithError {
    RetWithError(T value, Error error)
        : mValue(value)
        , mError(error)
    {
    }

    T     mValue;
    Error mError;
};

} // namespace aos

#endif
