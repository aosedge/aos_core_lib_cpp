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
class EnumStringer : public Stringer {
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
    constexpr EnumStringer(EnumType value)
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
    operator EnumType() const { return mValue; }

    /**
     * Casts to int.
     *
     * @return int.
     */
    operator int() const { return static_cast<int>(mValue); }

    /**
     * Compares if EnumStringer equals to another EnumStringer.
     *
     * @param stringer EnumStringer to compare with.
     * @return bool result.
     */
    bool operator==(const EnumStringer<T>& stringer) const { return GetValue() == stringer.GetValue(); };

    /**
     * Compares if EnumStringer doesn't equal to another EnumStringer.
     *
     * @param stringer EnumStringer to compare with.
     * @return bool result.
     */
    bool operator!=(const EnumStringer<T>& stringer) const { return GetValue() != stringer.GetValue(); };

    /**
     * Compares if EnumStringer equals to specified EnumStringer type.
     *
     * @param stringer EnumStringer to compare with.
     * @return bool result.
     */
    bool operator==(EnumType type) const { return GetValue() == type; };

    /**
     * Compares if EnumStringer doesn't equal to specified EnumStringer type.
     *
     * @param stringer EnumStringer to compare with.
     * @return bool result.
     */
    bool operator!=(EnumType type) const { return GetValue() != type; };

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
    const String ToString() const override
    {
        auto strings = T::GetStrings();

        if (static_cast<size_t>(mValue) < strings.Size()) {
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
