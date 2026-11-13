/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TOOLS_ENUM_HPP_
#define AOS_CORE_COMMON_TOOLS_ENUM_HPP_

#include "string.hpp"

namespace aos {

/**
 * Template used to convert enum to strings.
 */
template <class T>
class EnumStringer : public Stringer { // NOSONAR cpp:S1235 - virtual dtor would break constexpr array usage
public:
    using EnumType = typename T::Enum;

    /**
     * Construct a new EnumStringer object with default type.
     *
     * @param value curent enum value.
     */
    constexpr EnumStringer()
        : mValue(static_cast<EnumType>(0)) {};

    // cppcheck-suppress noExplicitConstructor
    // It is done for purpose to have possibility to assign enum type directly to EnumStringer.
    /**
     * Construct a new EnumStringer object with specified type.
     *
     * @param value curent enum value.
     */
    constexpr EnumStringer(EnumType value) // NOSONAR cpp:S1709
        : mValue(value) {};

    /**
     * Returns current enum value.
     *
     * @return value.
     */
    EnumType GetValue() const { return mValue; };

    /**
     * Casts to EnumType.
     *
     * @return EnumType.
     */
    operator EnumType() const // NOSONAR cpp:S1709
    {
        return mValue;
    }

    /**
     * Casts to int.
     *
     * @return int.
     */
    operator int32_t() const // NOSONAR cpp:S1709
    {
        return static_cast<int32_t>(mValue);
    }

    /**
     * Compares if EnumStringer equals to another EnumStringer.
     *
     * @param stringer EnumStringer to compare with.
     * @return bool result.
     */
    friend bool operator==(const EnumStringer& lhs, const EnumStringer<T>& stringer)
    {
        return lhs.GetValue() == stringer.GetValue();
    };

    /**
     * Compares if EnumStringer doesn't equal to another EnumStringer.
     *
     * @param stringer EnumStringer to compare with.
     * @return bool result.
     */
    friend bool operator!=(const EnumStringer& lhs, const EnumStringer<T>& stringer)
    {
        return lhs.GetValue() != stringer.GetValue();
    };

    /**
     * Compares if EnumStringer equals to specified EnumStringer type.
     *
     * @param stringer EnumStringer to compare with.
     * @return bool result.
     */
    friend bool operator==(const EnumStringer& lhs, EnumType type) { return lhs.GetValue() == type; };

    /**
     * Compares if EnumStringer doesn't equal to specified EnumStringer type.
     *
     * @param stringer EnumStringer to compare with.
     * @return bool result.
     */
    friend bool operator!=(const EnumStringer& lhs, EnumType type) { return lhs.GetValue() != type; };

    /**
     * Compares if specified EnumStringer type equals to EnumStringer.
     *
     * @param type specified EnumStringer type.
     * @param stringer EnumStringer to compare.
     * @return bool result.
     */
    friend bool operator==(EnumType type, const EnumStringer<T>& stringer) { return stringer.GetValue() == type; };

    /**
     * Compares if specified EnumStringer type doesn't equal to EnumStringer.
     *
     * @param type specified EnumStringer type.
     * @param stringer EnumStringer to compare.
     * @return bool result.
     */
    friend bool operator!=(EnumType type, const EnumStringer<T>& stringer) { return stringer.GetValue() != type; };

    /**
     * Converts enum value to string.
     *
     * @return string.
     */
    String ToString() const override
    {
        if (auto strings = T::GetStrings(); static_cast<size_t>(mValue) < strings.Size()) {
            return strings[static_cast<size_t>(mValue)];
        }

        return "unknown";
    }

    /**
     * Converts string to enum value.
     *
     * @param str string to convert.
     * @return Error.
     */
    Error FromString(const String& str)
    {
        auto strings = T::GetStrings();

        for (size_t i = 0; i < strings.Size(); i++) {
            if (str == strings[i]) {
                mValue = static_cast<EnumType>(i);

                return ErrorEnum::eNone;
            }
        }

        mValue = static_cast<EnumType>(strings.Size());

        return ErrorEnum::eNotFound;
    };

private:
    EnumType mValue;
};

} // namespace aos

#endif
