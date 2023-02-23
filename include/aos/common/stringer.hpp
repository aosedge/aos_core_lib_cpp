/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_STRINGER_HPP_
#define AOS_STRINGER_HPP_

#include "utils.hpp"

namespace aos {

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
    virtual const char* ToString() const = 0;
};

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
    EnumStringer()
        : mValue(static_cast<EnumType>(0)) {};

    // cppcheck-suppress noExplicitConstructor
    // It is done for purpose to have possibility to assign enum type directly to EnumStringer.
    /**
     * Construct a new EnumStringer object with specified type.
     *
     * @param value curent enum value.
     */
    EnumStringer(EnumType value)
        : mValue(value) {};

    /**
     * Returns current enum value.
     *
     * @return value.
     */
    EnumType GetValue() const { return mValue; };

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
    const char* ToString() const override
    {
        auto strings = T::GetStrings();

        if (strings.first == nullptr) {
            return nullptr;
        }

        if (static_cast<size_t>(mValue) < strings.second) {
            return strings.first[static_cast<size_t>(mValue)];
        }

        return "unknown";
    }

private:
    EnumType mValue;
};

} // namespace aos

#endif
