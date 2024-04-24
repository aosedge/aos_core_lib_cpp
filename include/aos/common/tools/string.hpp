/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_STRING_HPP_
#define AOS_STRING_HPP_

#define __STDC_FORMAT_MACROS

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "aos/common/tools/array.hpp"

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
        : Array(const_cast<char*>(str), str ? strlen(str) : 0)
    {
        if (str && *end()) {
            *end() = 0;
        }
    }

    /**
     * Constructs string from other string.
     *
     * @param str string.
     */
    String(const String& str)
        : Array(str)
    {
        if (*end()) {
            *end() = 0;
        }
    }

    /**
     * Assigns string to string.
     *
     * @param str string.
     * @return String&.
     */
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
        [[maybe_unused]] auto err = Array::Insert(end(), str.begin(), str.end());
        *end()                    = 0;
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
        *end()   = 0;

        return err;
    }

    /**
     * Removes range of characters from a string.
     *
     * @param from pointer to the first character to be removed from a string.
     * @param to past the end pointer of the input range to be removed.
     * @return Error.
     */
    Error Remove(char* from, char* to)
    {
        if (from < begin() || from > end()) {
            return ErrorEnum::eInvalidArgument;
        }

        if (to < begin() || to > end()) {
            return ErrorEnum::eInvalidArgument;
        }

        if (from > to) {
            return ErrorEnum::eInvalidArgument;
        }

        while (to != end()) {
            *from = *to;

            from++;
            to++;
        }

        return Resize(from - begin());
    }

    /**
     * Appends string operator.
     *
     * @param str string to append with.
     * @return String&.
     */
    String& operator+=(const String& str) { return Append(str); }

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
    friend bool operator==(const char* cStr, const String& str) { return str == String(cStr); };

    /**
     * Checks if C string doesn't equal to string.
     *
     * @param cStr C string to compare with.
     * @param srt string to compare with.
     * @return bool.
     */
    friend bool operator!=(const char* cStr, const String& str) { return str.operator!=(cStr); };

    /**
     * Converts sting to int.
     *
     * @return RetWithError<int>.
     */
    RetWithError<int> ToInt() const { return atoi(CStr()); }

    /**
     * Converts sting to uint64.
     *
     * @return RetWithError<uint64_t>.
     */
    RetWithError<uint64_t> ToUint64() const { return static_cast<uint64_t>(strtoull(CStr(), nullptr, 10)); }

    /**
     * Converts sting to int64.
     *
     * @return RetWithError<int64_t>.
     */
    RetWithError<int64_t> ToInt64() const { return static_cast<int64_t>(strtoll(CStr(), nullptr, 10)); }

    /**
     * Converts byte to hex string.
     *
     * @param val value to convert.
     * @param upperCase indicated if result should be in upper case.
     */
    Error ByteToHex(uint8_t val, bool upperCase = false)
    {
        constexpr char cDigits[] = "0123456789abcdef";

        auto high = cDigits[val >> 4];
        auto low  = cDigits[val & 0xF];

        auto err = Resize(2);
        if (!err.IsNone()) {
            return err;
        }

        (*this)[0] = upperCase ? toupper(high) : high;
        (*this)[1] = upperCase ? toupper(low) : low;

        return ErrorEnum::eNone;
    }

    /**
     * Converts hex string to byte.
     *
     * @return RetWithError<uint8_t> result.
     */
    RetWithError<uint8_t> HexToByte()
    {
        auto GetByte = [](char ch) -> RetWithError<uint8_t> {
            if (ch >= '0' && ch <= '9') {
                return static_cast<uint8_t>(ch - '0');
            }

            if (ch >= 'a' && ch <= 'f') {
                return static_cast<uint8_t>(ch - 'a' + 10);
            }

            if (ch >= 'A' && ch <= 'F') {
                return static_cast<uint8_t>(ch - 'A' + 10);
            }

            return {static_cast<uint8_t>(0), ErrorEnum::eInvalidArgument};
        };

        if (Size() > 2) {
            return {0, ErrorEnum::eNoMemory};
        }

        if (Size() == 0) {
            return {0, ErrorEnum::eInvalidArgument};
        }

        uint8_t result = 0;

        for (size_t i = 0; i < Size(); i++) {
            result <<= 4;

            auto byte = GetByte((*this)[i]);
            if (!byte.mError.IsNone()) {
                return {0, byte.mError};
            }

            result |= byte.mValue;
        }

        return result;
    }

    /**
     * Converts hex string to byte array.
     *
     * @param dst result byte array.
     * @return Error.
     */
    Error HexToByteArray(Array<uint8_t>& dst) const
    {
        if (dst.MaxSize() * 2 < Size()) {
            return ErrorEnum::eNoMemory;
        }

        dst.Clear();

        for (size_t i = 0; i < Size(); i += 2) {
            Error   err = ErrorEnum::eNone;
            uint8_t byte;

            char hex[] = {(*this)[i], '0', '\0'};

            if (i + 1 < Size()) {
                hex[1] = (*this)[i + 1];
            }

            Tie(byte, err) = String(hex, 2).HexToByte();
            if (!err.IsNone()) {
                return err;
            }

            dst.PushBack(byte);
        }

        return ErrorEnum::eNone;
    }

    /**
     * Converts byte array to a hex string.
     *
     * @param src source byte array.
     * @param upperCase use uppercase to represent hex value.
     * @return Error.
     */
    Error ByteArrayToHex(const Array<uint8_t>& src, bool upperCase = false)
    {
        Clear();

        for (const auto val : src) {
            char digits[3];

            auto err = String(digits, 2).ByteToHex(val, upperCase);
            if (!err.IsNone()) {
                return err;
            }

            err = Insert(end(), digits, static_cast<char*>(digits) + 2);
            if (!err.IsNone()) {
                return err;
            }
        }

        *end() = 0;

        return ErrorEnum::eNone;
    }

    /**
     * Returns byte array representation of the current string(null terminator excluded).
     *
     * @return Array<uint8_t>.
     */
    Array<uint8_t> AsByteArray() const { return Array<uint8_t>(reinterpret_cast<const uint8_t*>(CStr()), Size()); }

    /**
     * Converts error to string.
     *
     * @param inErr error.
     * @return Error.
     */
    Error Convert(const Error& inErr)
    {
        Clear();

        Append(inErr.Message());

        if (inErr.FileName()) {
            char tmpBuf[16];

            auto err = String(tmpBuf, sizeof(tmpBuf) - 1).Convert(inErr.LineNumber());
            if (!err.IsNone()) {
                return err;
            }

            Append(" (").Append(inErr.FileName()).Append(":").Append(tmpBuf).Append(")");
        }

        return ErrorEnum::eNone;
    }

    /**
     * Converts int to string.
     *
     * @param value int value.
     * @return Error.
     */
    Error Convert(int value) { return ConvertValue(value, "%d"); }

    /**
     * Converts uint64_t to string.
     *
     * @param value uint64_t value.
     * @return Error.
     */
    Error Convert(uint64_t value) { return ConvertValue(value, "%" PRIu64); }

    /**
     * Converts int64_t to string.
     *
     * @param value int64_t value.
     * @return Error.
     */
    Error Convert(int64_t value) { return ConvertValue(value, "%" PRIi64); }

    /**
     * Splits string to string list.
     *
     * @param list string list to split to.
     * @param delim delimeter.
     * @return Error.
     */
    template <typename T>
    Error Split(T& list, char delim = 0) const
    {
        list.Clear();

        auto it     = begin();
        auto prevIt = it;

        while (it != end()) {
            if (delim ? *it == delim : isspace(*it)) {
                auto err = list.PushBack(String(prevIt, it - prevIt));
                if (!err.IsNone()) {
                    return err;
                }

                it++;
                prevIt = it;

                continue;
            }

            it++;
        }

        if (it != prevIt) {
            auto err = list.PushBack(String(prevIt, it - prevIt));
            if (!err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Converts value to string according to format.
     *
     * @param value value.
     * @param format format.
     * @return Error.
     */
    template <typename T>
    Error ConvertValue(T value, const char* format)
    {
        return Format(format, value);
    }

    /**
     * Composes a string using format and additional argument list.
     *
     * @param format format string that follows specification of printf.
     * @param ...args additional arguments
     * @return Error.
     */
    template <typename... Args>
    Error Format(const char* format, Args... args)
    {
        Clear();

        // cppcheck-suppress wrongPrintfScanfArgNum
        auto ret = snprintf(Get(), MaxSize() + 1, format, args...);
        if (ret < 0) {
            return ret;
        }

        Resize(ret);

        return ErrorEnum::eNone;
    }

    /**
     * Looks for a substring in the current string and returns its starting position.
     * If substring is not found returns an error and index to the end of the string.
     *
     * @param startPos starting position for search.
     * @param substr substring to search.
     * @return RetWithError<size_t>.
     */
    RetWithError<size_t> FindSubstr(size_t startPos, const String& substr) const
    {
        for (size_t i = startPos; i < Size() - substr.Size(); i++) {
            const auto chunk = Array<char>(CStr() + i, substr.Size());

            if (chunk == substr) {
                return {i, ErrorEnum::eNone};
            }
        }

        return {Size(), ErrorEnum::eNotFound};
    }

    /**
     * Looks for any character from the symbol list and returns its position.
     * If character is not found returns an error and index to the end of the string.
     *
     * @param startPos starting position for search.
     * @param symbols symbols to search.
     * @return RetWithError<size_t>.
     */
    RetWithError<size_t> FindAny(size_t startPos, const String& symbols) const
    {
        for (size_t i = startPos; i < Size(); i++) {
            for (const auto ch : symbols) {
                if (ch == (*this)[i]) {
                    return {i, ErrorEnum::eNone};
                }
            }
        }

        return {Size(), ErrorEnum::eNotFound};
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
        *(static_cast<char*>(mBuffer.Get()))            = 0;
        *(static_cast<char*>(mBuffer.Get()) + cMaxSize) = 0;
        String::SetBuffer(mBuffer, cMaxSize);
    }

    /**
     * Creates static string from another static string.
     *
     * @param str string to create from.
     */
    StaticString(const StaticString& str)
        : String()
    {
        String::SetBuffer(mBuffer, cMaxSize);
        String::operator=(str);
    }

    /**
     * Assigns static string from another static string.
     *
     * @param str string to create from.
     */
    StaticString& operator=(const StaticString& str)
    {
        String::SetBuffer(mBuffer, cMaxSize);
        String::operator=(str);

        return *this;
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates static string from another string.
     *
     * @param str string to create from.
     */
    StaticString(const String& str)
    {
        String::SetBuffer(mBuffer, cMaxSize);
        String::operator=(str);
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates static string from C string.
     *
     * @param str initial value.
     */
    StaticString(const char* str)
    {
        String::SetBuffer(mBuffer, cMaxSize);
        String::operator=(str);
    }

    /**
     * Assigns C string to static string.
     *
     * @param str C string.
     * @return StaticString&.
     */
    StaticString& operator=(const char* str)
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
        *(static_cast<char*>(mBuffer.Get()))            = 0;
        *(static_cast<char*>(mBuffer.Get()) + cMaxSize) = 0;
        String::SetBuffer(mBuffer, cMaxSize);
    }

    /**
     * Creates dynamic string from another dynamic string.
     *
     * @param str string to create from.
     */
    DynamicString(const DynamicString& str)
        : String()
    {
        String::SetBuffer(mBuffer, cMaxSize);
        String::operator=(str);
    }

    /**
     * Assigns dynamic string from another dynamic string.
     *
     * @param str string to create from.
     */
    DynamicString& operator=(const DynamicString& str)
    {
        String::SetBuffer(mBuffer, cMaxSize);
        String::operator=(str);

        return *this;
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates dynamic string from another string.
     *
     * @param str initial value.
     */
    DynamicString(const String& str)
    {
        String::SetBuffer(mBuffer, cMaxSize);
        String::operator=(str);
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates dynamic string from C string.
     *
     * @param str initial value.
     */
    DynamicString(const char* str)
    {
        String::SetBuffer(mBuffer, cMaxSize);
        String::operator=(str);
    }

    /**
     * Assigns C string to dynamic string.
     *
     * @param str C string.
     * @return DynamicString&.
     */
    DynamicString& operator=(const char* str)
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
