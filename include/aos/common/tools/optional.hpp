/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OPTIONAL_HPP_
#define AOS_OPTIONAL_HPP_

#include <assert.h>
#include <cstdint>

namespace aos {

/**
 * Optional instance.
 *
 * @tparam T
 */
template <class T>
class Optional {
public:
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
     * @return void.
     */
    template <typename... Args>
    void EmplaceValue(Args... args)
    {
        if (HasValue()) {
            *reinterpret_cast<T*>(mBuffer) = T(args...);
        } else {
            ::new (static_cast<void*>(mBuffer)) T(args...);
            mHasValue = true;
        }
    }

    /**
     * Destroys contained value.
     *
     * @return void.
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
     * Destroys optional instance.
     */
    ~Optional() { Reset(); }

private:
    alignas(T) uint8_t mBuffer[sizeof(T)];
    bool mHasValue = false;
};

} // namespace aos

#endif
