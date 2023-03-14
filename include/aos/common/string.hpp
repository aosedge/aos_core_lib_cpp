/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_STRING_HPP_
#define AOS_STRING_HPP_

#include <aos/common/array.hpp>
#include <stdio.h>
#include <string.h>

namespace aos {

/**
 * String instance.
 */
class String : public Array<char> {
public:
    /**
     * Creates string.
     */
    using Array::Array;

    // TODO: automatically make const String from const char*.
    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs string from C string.
     *
     * @param str C string.
     */
    String(const char* str)
        : Array(const_cast<char*>(str), strlen(str))
    {
        if (*end()) {
            *end() = 0;
        }
    }

    String(const String& str)
        : Array(str)
    {
        if (*end()) {
            *end() = 0;
        }
    }

    String& operator=(const String& str)
    {
        Array::operator=(str);
        *end() = 0;

        return *this;
    }

    /**
     * Returns C string representation.
     *
     * @return const char* C string.
     */
    const char* CStr() const { return Get(); }

    /**
     * Sets new string size.
     *
     * @param size new size.
     * @return Error.
     */
    Error Resize(size_t size)
    {
        if (size > MaxSize()) {
            return ErrorEnum::eNoMemory;
        }

        Array::SetSize(size);
        *end() = 0;

        return ErrorEnum::eNone;
    }

    /**
     * Clears string.
     */
    void Clear() { Resize(0); }

    /**
     * Appends string.
     *
     * @param str string to append with.
     * @return String&.
     */
    String& Append(const String& str)
    {
        auto err = Array::Insert(end(), str.begin(), str.end());
        *end() = 0;
        assert(err.IsNone());

        return *this;
    }

    /**
     * Inserts items from range.
     *
     * @param pos insert position.
     * @param from insert from this position.
     * @param till insert till this position.
     * @return Error.
     */
    Error Insert(char* pos, const char* from, const char* till)
    {
        auto err = Array::Insert(pos, from, till);
        *end() = 0;

        return err;
    }

    /**
     * Appends string operator.
     *
     * @param str string to append with.
     * @return String&.
     */
    String& operator+=(const String& str) { return Append(str); }

    /**
     * Checks if str equals to C string.
     *
     * @param cStr C string to compare with.
     * @return bool.
     */
    bool operator==(const char* cStr) const
    {
        if (strlen(cStr) != Size()) {
            return false;
        }

        return memcmp(Get(), cStr, Size()) == 0;
    };

    /**
     * Checks if str doesn't equal to C string.
     *
     * @param cStr C string to compare with.
     * @return bool.
     */
    bool operator!=(const char* cStr) const { return !operator==(cStr); };

    /**
     * Checks if str equals to another string.
     *
     * @param str string to compare with.
     * @return bool.
     */
    bool operator==(const String& str) const { return Array::operator==(str); };

    /**
     * Checks if str doesn't equal to another string.
     *
     * @param str string to compare with.
     * @return bool.
     */
    bool operator!=(const String& str) const { return Array::operator!=(str); };

    /**
     * Checks if C string equals to string.
     *
     * @param cStr C string to compare with.
     * @param srt string to compare with.
     * @return bool.
     */
    friend bool operator==(const char* cStr, const String& str) { return str.operator==(cStr); };

    /**
     * Checks if C string doesn't equal to string.
     *
     * @param cStr C string to compare with.
     * @param srt string to compare with.
     * @return bool.
     */
    friend bool operator!=(const char* cStr, const String& str) { return str.operator!=(cStr); };

    /**
     * Converts int to string.
     *
     * @param value int value.
     * @return Error.
     */
    String& Convert(int value)
    {
        auto ret = snprintf(Get(), MaxSize() + 1, "%d", value);
        assert(ret >= 0 && static_cast<size_t>(ret) <= MaxSize());

        Resize(ret);

        return *this;
    }

    /**
     * Converts error to string.
     *
     * @param err error.
     * @return Error.
     */
    String& Convert(const Error& err)
    {
        Clear();

        Append(err.Message());

        if (err.FileName()) {
            char tmpBuf[16];

            Append(" (")
                .Append(err.FileName())
                .Append(":")
                .Append(String(tmpBuf, sizeof(tmpBuf) - 1).Convert(err.LineNumber()))
                .Append(")");
        }

        return *this;
    }
};

/**
 * Static string instance.
 *
 * @tparam cMaxSize max static string size.
 */
template <size_t cMaxSize>
class StaticString : public String {
public:
    /**
     * Creates static string.
     */
    StaticString()
    {
        *(static_cast<char*>(mBuffer.Get())) = 0;
        *(static_cast<char*>(mBuffer.Get()) + cMaxSize) = 0;
        String::SetBuffer(mBuffer, 0, cMaxSize);
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates static string from another string.
     *
     * @param str initial value.
     */
    StaticString(const String& str)
    {
        String::SetBuffer(mBuffer, 0, cMaxSize);
        String::operator=(str);
    }

    /**
     * Assigns string to static string.
     *
     * @param str string to assign.
     * @return StaticString&.
     */
    StaticString& operator=(const String& str)
    {
        String::operator=(str);

        return *this;
    }

private:
    StaticBuffer<cMaxSize + 1> mBuffer;
};

/**
 * Dynamic string instance.
 *
 * @tparam cMaxSize max dynamic string size.
 */
template <size_t cMaxSize>
class DynamicString : public String {
public:
    /**
     * Create dynamic string.
     */
    DynamicString()
        : mBuffer(cMaxSize * +1)
    {
        *(static_cast<char*>(mBuffer.Get())) = 0;
        *(static_cast<char*>(mBuffer.Get()) + cMaxSize) = 0;
        String::SetBuffer(mBuffer, 0, cMaxSize);
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates dynamic string from another string.
     *
     * @param str initial value.
     */
    DynamicString(const String& str)
    {
        String::SetBuffer(mBuffer, 0, cMaxSize);
        String::operator=(str);
    }

    /**
     * Assigns string to static string.
     *
     * @param str string to assign.
     * @return DynamicString&.
     */
    DynamicString& operator=(const String& str)
    {
        String::operator=(str);

        return *this;
    }

private:
    DynamicBuffer mBuffer;
};

/**
 * Interface used to convert derived type to string.
 */
class Stringer {
public:
    /**
     * Returns string representation of derived class.
     *
     * @return string.
     */
    virtual const String ToString() const = 0;
};

} // namespace aos

#endif
