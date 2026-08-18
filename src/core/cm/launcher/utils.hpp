/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_UTILS_HPP_
#define AOS_CORE_CM_LAUNCHER_UTILS_HPP_

#include <core/common/tools/array.hpp>

namespace aos::cm::launcher {

template <typename It, class Cmp>
class FilterIterator {
public:
    FilterIterator(It it, It end, Cmp cmp)
        : mIt(it)
        , mEnd(end)
        , mCmp(cmp)
    {
        while (mIt != mEnd && !mCmp(*mIt)) {
            ++mIt;
        }
    }

    FilterIterator& operator++()
    {
        assert(mIt != mEnd);

        ++mIt;

        while (mIt != mEnd && !mCmp(*mIt)) {
            ++mIt;
        }

        return *this;
    }

    FilterIterator operator++(int)
    {
        assert(mIt != mEnd);

        FilterIterator tmp = *this;

        ++(*this);

        return tmp;
    }

    bool operator==(const FilterIterator& other) const { return mIt == other.mIt; }
    bool operator!=(const FilterIterator& other) const { return mIt != other.mIt; }

    auto& operator*() const { return *mIt; }
    auto  operator->() const { return mIt; }

private:
    It  mIt;
    It  mEnd;
    Cmp mCmp;
};

template <typename It, class Cmp>
class Filter {
public:
    Filter(It begin, It end, Cmp cmp)
        : mBegin(begin)
        , mEnd(end)
        , mCmp(cmp)
    {
    }

    template <typename T>
    Filter(Array<T>& array, Cmp cmp)
        : Filter(array.begin(), array.end(), cmp)
    {
    }

    template <typename T>
    Filter(const Array<T>& array, Cmp cmp)
        : Filter(array.begin(), array.end(), cmp)
    {
    }

    FilterIterator<It, Cmp> begin() const { return FilterIterator<It, Cmp>(mBegin, mEnd, mCmp); }
    FilterIterator<It, Cmp> end() const { return FilterIterator<It, Cmp>(mEnd, mEnd, mCmp); }

private:
    It  mBegin;
    It  mEnd;
    Cmp mCmp;
};

template <typename T, typename Cmp>
Filter(Array<T>&, Cmp) -> Filter<typename Array<T>::Iterator, Cmp>;

template <typename T, typename Cmp>
Filter(const Array<T>&, Cmp) -> Filter<typename Array<T>::ConstIterator, Cmp>;

} // namespace aos::cm::launcher

#endif
