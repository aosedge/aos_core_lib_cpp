/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ERROR_HPP_
#define AOS_ERROR_HPP_

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "aos/common/config.hpp"
#include "aos/common/tools/utils.hpp"

namespace aos {

/**
 * Returns only base name from file path.
 */

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/**
 * Wraps aos:Error with file name and line number information.
 */
#define AOS_ERROR_WRAP(err) aos::Error(err, nullptr, __FILENAME__, __LINE__)

/**
 * Maximum error message length.
 */
constexpr static auto cMaxErrorStrLen = AOS_CONFIG_TOOLS_ERROR_STR_LEN;

/**
 * Aos errors.
 */
class Error {
public:
    // NOTE: new error type should be added also to private GetStrings() method below and covered
    // with unit test: TEST(CommonTest, ErrorMessages).
    /**
     * Error enum.
     */
    enum class Enum {
        eNone,
        eFailed,
        eRuntime,
        eNoMemory,
        eOutOfRange,
        eNotFound,
        eInvalidArgument,
        eTimeout,
        eAlreadyExist,
        eWrongState,
        eInvalidChecksum,
        eAlreadyLoggedIn,
        eNotSupported,
        eNumErrors
    };

    /**
     * Constructs default error instance.
     */
    Error()
        : mErr(Enum::eNone)
        , mErrno(0)
        , mFileName(nullptr)
        , mLineNumber(0)
    {
        mMessage[0] = 0;
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs error instance from ErrorType::Enum.
     */
    Error(Enum err, const char* msg = nullptr, const char* fileName = nullptr, int lineNumber = 0)
        : mErr(err)
        , mErrno(0)
        , mFileName(fileName)
        , mLineNumber(lineNumber)
    {
        CopyMessage(msg);
    }

    /**
     * Constructs error instance from another error.
     */
    Error(const Error& err, const char* msg = nullptr, const char* fileName = nullptr, int lineNumber = 0)
        : mErr(err.mErr)
        , mErrno(err.mErrno)
        , mFileName(fileName)
        , mLineNumber(lineNumber)
    {
        if (err.mFileName) {
            mFileName   = err.mFileName;
            mLineNumber = err.mLineNumber;
        }

        if (msg) {
            CopyMessage(msg);
        } else {
            CopyMessage(err.mMessage);
        }
    }

    /**
     * Assigns error from another error.
     *
     * @param err error to copy from.
     */
    Error& operator=(const Error& err)
    {
        mErr   = err.mErr;
        mErrno = err.mErrno;

        mFileName   = err.mFileName;
        mLineNumber = err.mLineNumber;

        CopyMessage(err.mMessage);

        return *this;
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs error instance from system errno value.
     *
     * @param e errno.
     */
    Error(int errNo, const char* msg = nullptr, const char* fileName = nullptr, int lineNumber = 0)
        : mErr(errNo == 0 ? Enum::eNone : Enum::eRuntime)
        , mErrno(errNo < 0 ? -errNo : errNo)
        , mFileName(fileName)
        , mLineNumber(lineNumber)
    {
        CopyMessage(msg);
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
    const char* Message() const { return mMessage; }

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
     * Returns errno string.
     *
     * @return const char* errno string.
     */
    const char* StrErrno() const
    {
        if (mErrno != 0) {
            return strerror(mErrno);
        }

        return nullptr;
    }

    /**
     * Returns error string.
     *
     * @return const char* error string.
     */
    const char* StrValue() const
    {
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
     * Compares if error equals to another error value.
     *
     * @param err error to compare with.
     * @return bool result.
     */
    bool operator==(const Error& err) const { return mErr == err.mErr; };

    /**
     * Compares if error doesn't equal to another error value.
     *
     * @param err error to compare with.
     * @return bool result.
     */
    bool operator!=(const Error& err) const { return mErr != err.mErr; };

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
    constexpr static size_t cMaxMessageLen = AOS_CONFIG_TOOLS_ERROR_MESSAGE_LEN + 1;

    /**
     * Copies error message.
     *
     * @param msg error message.
     */
    void CopyMessage(const char* msg)
    {
        if (msg != nullptr) {
            snprintf(mMessage, sizeof(mMessage), "%s", msg);

            return;
        }

        mMessage[0] = '\0';
    }

    /**
     * Returns error string array.
     *
     * @return Pair<const char* const*, size_t> error string array.
     */
    static Pair<const char* const*, size_t> GetStrings()
    {
        static const char* const sErrorTypeStrings[] = {
            "none",
            "failed",
            "runtime error",
            "not enough memory",
            "out of range",
            "not found",
            "invalid argument",
            "timeout",
            "already exist",
            "wrong state",
            "invalid checksum",
            "already logged in",
            "not supported",
        };

        return Pair<const char* const*, size_t>(sErrorTypeStrings, ArraySize(sErrorTypeStrings));
    };

    Enum        mErr;
    int         mErrno;
    const char* mFileName;
    int         mLineNumber;
    char        mMessage[cMaxMessageLen];
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
    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs return value with error instance.
     *
     * @param value return value.
     * @param error return error.
     */
    RetWithError(const T& value, const Error& error = ErrorEnum::eNone)
        : mValue(value)
        , mError(error)
    {
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs return value with error instance.
     *
     * @param value return value.
     * @param error return error.
     */
    RetWithError(T&& value, const Error& error = ErrorEnum::eNone)
        : mValue(Move(value))
        , mError(error)
    {
    }

    /**
     * Comparison operators.
     */
    bool operator==(const RetWithError<T>& other) const { return mValue == other.mValue && mError == other.mError; }
    bool operator!=(const RetWithError<T>& other) const { return !(*this == other); }

    /**
     * Holds returned value.
     */
    T mValue;

    /**
     * Holds returned error.
     */
    Error mError;
};

/**
 * Specialization for references.
 *
 * @tparam T value type.
 */
template <typename T>
struct RetWithError<T&> {
    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs return value with error instance.
     *
     * @param value return value.
     * @param error return error.
     */
    RetWithError(T& value, const Error& error = ErrorEnum::eNone)
        : mValue(value)
        , mError(error)
    {
    }

    /**
     * Holds returned value.
     */
    T& mValue;

    /**
     * Holds returned error.
     */
    Error mError;
};

/**
 * Helper class for Tie method implementation.
 *
 * @tparam T value type.
 */
template <typename T>
struct TieWrapper {
    /**
     * Constructs wrapper using references on a value and error.
     *
     * @param value tied value.
     * @param error tied error.
     */
    TieWrapper(T& value, Error& error)
        : mValue(value)
        , mError(error)
    {
    }

    /**
     * Assignment operator.
     *
     * @param src return value.
     * @return TieWrapper<T>&
     */
    template <typename U>
    TieWrapper<T>& operator=(const RetWithError<U>& src)
    {
        mValue = src.mValue;
        mError = src.mError;

        return *this;
    }

    /**
     * Assignment operator.
     *
     * @param src return value.
     * @return TieWrapper<T>&
     */
    template <typename U>
    TieWrapper<T>& operator=(RetWithError<U>&& src)
    {
        mValue = Move(src.mValue);
        mError = src.mError;

        return *this;
    }

    /**
     * Holds returned value.
     */
    T& mValue;

    /**
     * Holds returned error.
     */
    Error& mError;
};

/**
 * Creates a pair from provided references.
 *
 * @param value value reference.
 * @param error error reference.
 * @return TieWrapper<T>.
 */
template <typename T>
TieWrapper<T> Tie(T& value, Error& error)
{
    return TieWrapper<T>(value, error);
}

} // namespace aos

#endif
