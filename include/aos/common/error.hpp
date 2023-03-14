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

#include "aos/common/utils.hpp"

namespace aos {

/**
 * Returns only base name from file path.
 */

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/**
 * Wraps aos:Error with file name and line number information.
 */
#define AOS_ERROR_WRAP(err) aos::Error(err, __FILENAME__, __LINE__)

/**
 * Aos errors.
 */
class Error {
public:
    /**
     * Error enum.
     */
    enum class Enum { eNone, eFailed, eRuntime, eNoMemory, eOutOfRange, eNotFound, eInvalidArgument, eNumErrors };

    /**
     * Constructs default error instance.
     */
    Error()
        : mErr(Enum::eNone)
        , mErrno(0)
        , mFileName(nullptr)
        , mLineNumber(0)
    {
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs error instance from ErrorType::Enum.
     */
    Error(Enum err, const char* fileName = nullptr, int lineNumber = 0)
        : mErr(err)
        , mErrno(0)
        , mFileName(fileName)
        , mLineNumber(lineNumber)
    {
    }

    /**
     * Constructs error instance from another error.
     */
    Error(const Error& err, const char* fileName = nullptr, int lineNumber = 0)
        : mErr(err.mErr)
        , mErrno(err.mErrno)
        , mFileName(fileName)
        , mLineNumber(lineNumber)
    {
        if (!mFileName) {
            mFileName = err.mFileName;
            mLineNumber = err.mLineNumber;
        }
    }

    /**
     * Assigns error from another error.
     *
     * @param err error to copy from.
     */
    Error& operator=(const Error& err)
    {
        mErr = err.mErr;
        mErrno = err.mErrno;

        if (!mFileName) {
            mFileName = err.mFileName;
            mLineNumber = err.mLineNumber;
        }

        return *this;
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs error instance from system errno value.
     *
     * @param e errno.
     */
    Error(int errNo, const char* fileName = nullptr, int lineNumber = 0)
        : mErr(errNo == 0 ? Enum::eNone : Enum::eRuntime)
        , mErrno(errNo)
        , mFileName(fileName)
        , mLineNumber(lineNumber)
    {
    }

    /**
     * Checks if error is none.
     */
    bool IsNone() const { return mErr == Enum::eNone; }

    /**
     * Checks if error has specified type.
     *
     * @param e error to check.
     * @return bool result.
     */
    bool Is(const Error& err) const
    {
        if (mErrno != 0) {
            return mErrno == err.mErrno;
        }

        return mErr == err.mErr;
    }

    /**
     * Returns error enum.
     *
     * @return Error::Enum.
     */
    Error::Enum Value() const { return mErr; };

    /**
     * Returns error message.
     *
     * @return const char* error message.
     */
    const char* Message() const
    {
        if (mErrno != 0) {
            return strerror(mErrno);
        }

        auto strings = GetStrings();

        if (strings.mFirst == nullptr) {
            return nullptr;
        }

        if (static_cast<size_t>(mErr) < strings.mSecond) {
            return strings.mFirst[static_cast<size_t>(mErr)];
        }

        return "unknown";
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

    /**
     * Returns error line number.
     *
     * @return int line number.
     */
    int LineNumber() const { return mLineNumber; }

    /**
     * Compares if error equals to specified error value.
     *
     * @param err error value to compare with.
     * @return bool result.
     */
    bool operator==(Enum value) const { return mErr == value; };

    /**
     * Compares if error doesn't equal to specified error value.
     *
     * @param err error value to compare with.
     * @return bool result.
     */
    bool operator!=(Enum value) const { return mErr != value; };

    /**
     * Compares if specified error value equals to error.
     *
     * @param value specified error value.
     * @param err error to compare.
     * @return bool result.
     */
    friend bool operator==(Enum value, const Error& err) { return err.mErr == value; };

    /**
     * Compares if specified error value doesn't equal to error.
     *
     * @param value specified error value.
     * @param err error to compare.
     * @return bool result.
     */
    friend bool operator!=(Enum value, const Error& err) { return err.mErr != value; };

private:
    /**
     * Returns error string array.
     *
     * @return Pair<const char* const*, size_t> error string array.
     */
    static Pair<const char* const*, size_t> GetStrings()
    {
        static const char* const cErrorTypeStrings[static_cast<size_t>(Enum::eNumErrors)]
            = {"none", "failed", "runtime error", "not enough memory", "out of range", "invalid argument", "not found"};

        return Pair<const char* const*, size_t>(cErrorTypeStrings, static_cast<size_t>(Enum::eNumErrors));
    };

    Enum        mErr;
    int         mErrno;
    const char* mFileName;
    int         mLineNumber;
};

/**
 * Error enum.
 */
using ErrorEnum = Error::Enum;

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
