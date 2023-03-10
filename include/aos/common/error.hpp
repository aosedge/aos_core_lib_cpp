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
 * Returns only base name from file path.
 */

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/**
 * Creates AOS_ERROR with file name and line number information.
 */
#define AOS_ERROR(err) aos::Error(err, __FILENAME__, __LINE__)

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
        , mFileName(nullptr)
        , mLineNumber(0)
    {
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs error instance from ErrorType::Enum.
     */
    Error(ErrorEnum err, const char* fileName = nullptr, int lineNumber = 0)
        : EnumStringer(err)
        , mErrno(0)
        , mFileName(fileName)
        , mLineNumber(lineNumber)
    {
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs error instance from system errno value.
     *
     * @param e errno.
     */
    Error(int errNo, const char* fileName = nullptr, int lineNumber = 0)
        : EnumStringer(errNo == 0 ? ErrorEnum::eNone : ErrorEnum::eRuntime)
        , mErrno(errNo)
        , mFileName(fileName)
        , mLineNumber(lineNumber)
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

    /**
     * Returns errno
     * @return int
     */
    int Errno() const { return mErrno; }

    /**
     * Returns error file name.
     *
     * @return const char* file name.
     */
    const char* FileName() const { return mFileName; }

    int LineNumber() const { return mLineNumber; }

private:
    int         mErrno;
    const char* mFileName;
    int         mLineNumber;
};

/**
 * Container that holds value and return error.
 *
 * @tparam T value type.
 */
template <typename T>
struct RetWithError {
    /**
     * Constructs return value with error instance.
     *
     * @param value return value.
     * @param error return error.
     */
    RetWithError(T value, const Error& error)
        : mValue(value)
        , mError(error)
    {
    }

    T     mValue;
    Error mError;
};

} // namespace aos

#endif
