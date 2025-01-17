/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ALGORITHM_HPP_
#define AOS_ALGORITHM_HPP_

#include "aos/common/tools/error.hpp"

namespace aos {

/**
 * Algorithm interface.
 * @tparam T value type.
 * @tparam I iterator type.
 * @tparam CI const iterator type.
 */
template <typename T, typename I, typename CI>
class AlgorithmItf {
public:
    /**
     * Returns current container size.
     *
     * @return size_t.
     */
    virtual size_t Size() const = 0;

    /**
     * Returns maximum available container size.
     *
     * @return size_t.
     */
    virtual size_t MaxSize() const = 0;

    // Used for range based loop.
    virtual I  begin(void)       = 0;
    virtual I  end(void)         = 0;
    virtual CI begin(void) const = 0;
    virtual CI end(void) const   = 0;

    /**
     * Removes item from container.
     *
     * @param item item to remove.
     * @return RetWithError<I> pointer to next after deleted item.
     */
    virtual I Erase(CI it) = 0;

    /**
     * Removes item range from container.
     *
     * @param first first item to remove.
     * @param last  lst item to remove.
     * @return RetWithError<I> pointer to next after deleted item.
     */
    virtual I Erase(CI first, CI last) = 0;

    /**
     * Checks if container is empty.
     *
     * @return bool.
     */
    bool IsEmpty() const { return Size() == 0; }

    /**
     * Checks if container is full.
     *
     * @return bool.
     */

    bool IsFull() const { return Size() == MaxSize(); }

    /**
     * Returns true if container is not empty.
     *
     * @return bool.
     */
    explicit operator bool() const { return Size() > 0; }

    /**
     * Checks if container equals to another container.
     *
     * @param container to compare with.
     * @return bool.
     */
    template <typename C>
    bool operator==(const C& container) const
    {
        if (container.Size() != Size()) {
            return false;
        }

        for (auto it1 = begin(), it2 = container.begin(); it1 != end(); it1++, it2++) {
            if (!(*it1 == *it2)) {
                return false;
            }
        }

        return true;
    }

    /**
     * Checks if container doesn't equal to another container.
     *
     * @param container to compare with.
     * @return bool.
     */
    template <typename C>
    bool operator!=(const C& container) const
    {
        return !operator==(container);
    }

    /**
     * Returns container first item.
     *
     * @return RetWithError<T&>.
     */
    RetWithError<T&> Front()
    {
        if (IsEmpty()) {
            return {*end(), ErrorEnum::eNotFound};
        }

        return *begin();
    }

    /**
     * Returns container first const item.
     *
     * @return RetWithError<const T&>.
     */
    RetWithError<const T&> Front() const
    {
        if (IsEmpty()) {
            return {*end(), ErrorEnum::eNotFound};
        }

        return *begin();
    }

    /**
     * Returns container last item.
     *
     * @return RetWithError<T&>.
     */
    RetWithError<T&> Back()
    {
        if (IsEmpty()) {
            return {*end(), ErrorEnum::eNotFound};
        }

        auto it = end();

        return *(--it);
    }

    /**
     * Returns container last const item.
     *
     * @return RetWithError<const T&>.
     */
    RetWithError<const T&> Back() const
    {
        if (IsEmpty()) {
            return {*end(), ErrorEnum::eNotFound};
        }

        auto it = end();

        return *(--it);
    }

    /**
     * Finds const element in container.
     *
     * @param value value to find.
     * @return CI.
     */
    CI Find(const T& value) const
    {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }

        return end();
    }

    /**
     * Finds element in container.
     *
     * @param value value to find.
     * @return I.
     */
    I Find(const T& value)
    {
        for (auto it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }

        return end();
    }

    /**
     * Finds const element in container that match argument.
     *
     * @param match match function.
     * @return CI.
     */
    template <typename P>
    CI FindIf(P match) const
    {
        for (auto it = begin(); it != end(); ++it) {
            if (match(*it)) {
                return it;
            }
        }

        return end();
    }

    /**
     * Finds element in container that match argument.
     *
     * @param match match function.
     * @return I.
     */
    template <typename P>
    I FindIf(P match)
    {
        for (auto it = begin(); it != end(); it++) {
            if (match(*it)) {
                return it;
            }
        }

        return end();
    }
    /**
     * Removes element from container.
     *
     * @param value value to remove.
     * @return size_t.
     */
    size_t Remove(const T& value)
    {
        size_t count = 0;

        for (auto it = begin(); it != end();) {
            if (*it == value) {
                it = Erase(it);
                count++;
            } else {
                it++;
            }
        }

        return count;
    }

    /**
     * Removes element from container that match argument.
     *
     * @param match match function.
     * @return size_t.
     */
    template <typename P>
    size_t RemoveIf(P match)
    {
        size_t count = 0;

        for (auto it = begin(); it != end();) {
            if (match(*it)) {
                it = Erase(it);
                count++;
            } else {
                it++;
            }
        }

        return count;
    }

    /*
     * Sorts container items using sort function.
     *
     * @tparam F type of sort function.
     * @param sortFunc sort function.
     */
    template <typename F>
    void Sort(F sortFunc)
    {
        for (auto it1 = begin(); it1 != end(); it1++) {
            for (auto it2 = begin(); it2 != end(); it2++) {
                if (sortFunc(*it1, *it2)) {
                    auto tmp = *it1;

                    *it1 = *it2;
                    *it2 = tmp;
                }
            }
        }
    }

    /**
     * Sorts container items using default comparision operator.
     */
    void Sort()
    {
        Sort([](const T& val1, const T& val2) { return val1 < val2; });
    }
};

} // namespace aos

#endif
