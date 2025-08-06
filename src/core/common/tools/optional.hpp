/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TOOLS_OPTIONAL_HPP_
#define AOS_CORE_COMMON_TOOLS_OPTIONAL_HPP_

#include <assert.h>
#include <cstdint>

#include "log.hpp"

namespace aos {

/**
 * Optional instance.
 *
 * @tparam T
 */
template <class T>
class Optional {
public:
    // cppcheck-suppress uninitMemberVar
    /**
     * Default constructor.
     */
    Optional() = default;

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates optional instance and initializes it with the value object.
     *
     * @param value.
     */
    Optional(const T& value) { SetValue(value); }

    /**
     * Copy constructor.
     *
     * @param other.
     */
    Optional(const Optional& other) { *this = other; }

    /**
     * Copy operator.
     *
     * @param other.
     * @return Optional.
     */
    Optional& operator=(const Optional& other)
    {
        if (other.HasValue()) {
            SetValue(other.GetValue());
        } else {
            Reset();
        }
        return *this;
    }

    /**
     * Tests whether current optional contains value.
     *
     * @return bool.
     */
    bool HasValue() const { return mHasValue; }

    /**
     * Returns reference to a contained value.
     *
     * @return T&.
     */
    T& GetValue()
    {
        assert(HasValue());
        // cppcheck-suppress invalidPointerCast
        return *reinterpret_cast<T*>(mBuffer);
    }

    /**
     * Returns reference to a contained value.
     *
     * @return const T&.
     */
    const T& GetValue() const
    {
        assert(HasValue());
        // cppcheck-suppress invalidPointerCast
        return *reinterpret_cast<const T*>(mBuffer);
    }

    /**
     * Assigns value.
     *
     * @param value
     * @return void.
     */
    void SetValue(const T& value)
    {
        if (HasValue()) {
            // cppcheck-suppress invalidPointerCast
            *reinterpret_cast<T*>(mBuffer) = value;
        } else {
            ::new (static_cast<void*>(mBuffer)) T(value);
            mHasValue = true;
        }
    }

    /**
     * Assigns value.
     *
     * @param value
     */
    template <typename... Args>
    void EmplaceValue(Args... args)
    {
        if (HasValue()) {
            GetValue().~T();
        }

        ::new (static_cast<void*>(mBuffer)) T(args...);
        mHasValue = true;
    }

    /**
     * Destroys contained value.
     */
    void Reset()
    {
        if (HasValue()) {
            GetValue().~T();
        }
        mHasValue = false;
    }

    /**
     * Compares optional instances.
     *
     * @param other instance to compare with.
     * @return bool.
     */
    bool operator==(const Optional& other) const
    {
        if (!HasValue() && !other.HasValue()) {
            return true;
        }

        if (HasValue() != other.HasValue()) {
            return false;
        }

        return GetValue() == other.GetValue();
    }

    /**
     * Compares optional instances.
     *
     * @param other instance to compare with.
     * @return bool.
     */
    bool operator!=(const Optional& other) const { return !operator==(other); }

    /**
     * Returns pointer to contained value.
     *
     * @return T*.
     */
    T* operator->() { return &GetValue(); }

    /**
     * Returns pointer to contained value.
     *
     * @return T*.
     */
    const T* operator->() const { return &GetValue(); }

    /**
     * Dereferences holding object.
     *
     * @return T&.
     */
    T& operator*() { return GetValue(); }

    /**
     * Dereferences holding object.
     *
     * @return T&.
     */
    const T& operator*() const { return GetValue(); }

    /**
     * Outputs optional object to log.
     *
     * @param log log to output.
     * @param obj optional object.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const Optional& obj)
    {
        if (!obj.HasValue()) {
            return log << "none";
        }

        return log << *obj;
    }

    /**
     * Destroys optional instance.
     */
    ~Optional() { Reset(); }

private:
    alignas(T) uint8_t mBuffer[sizeof(T)];
    bool mHasValue = false;
};

} // namespace aos

#endif
