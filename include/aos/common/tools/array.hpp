/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ARRAY_HPP_
#define AOS_ARRAY_HPP_

#include <assert.h>

#include "aos/common/tools/buffer.hpp"
#include "aos/common/tools/error.hpp"
#include "aos/common/tools/new.hpp"

namespace aos {

/**
 * Array instance.
 * @tparam T array type.
 */
template <typename T>
class Array {
public:
    /**
     * Crates empty array instance.
     */
    Array() = default;

    /**
     * Crates array instance over the buffer.
     *
     * @param buffer underlying buffer.
     * @param size current array size.
     */
    explicit Array(const Buffer& buffer) { SetBuffer(buffer); }

    /**
     * Creates array instance from C array.
     *
     * @param items const C array.
     * @param size C array size.
     */
    Array(const T* items, size_t size)
        : mItems(const_cast<RemoveConstType<T>*>(items))
        , mSize(size)
        , mMaxSize(size)
    {
    }

    /**
     * Creates array instance from another array.
     *
     * @param array another array instance.
     */
    Array(const Array& array) = default;

    /**
     * Assigns existing array to the current one.
     *
     * @param array existing array.
     * @return Array&.
     */
    Array& operator=(const Array& array)
    {
        assert(mItems && array.mSize <= mMaxSize);

        mSize = array.mSize;

        if (mItems == array.mItems) {
            return *this;
        }

        for (size_t i = 0; i < array.Size(); i++) {
            new (&mItems[i]) T(array.mItems[i]);
        }

        return *this;
    }

    /**
     * Clears array.
     */
    void Clear()
    {
        for (auto it = begin(); it != end(); it++) {
            it->~T();
        }

        mSize = 0;
    }

    /**
     * Checks if array is empty.
     *
     * @return bool.
     */
    bool IsEmpty() const { return mSize == 0; }

    /**
     * Checks if arrays is full.
     *
     * @return bool.
     */
    bool IsFull() const { return mSize == mMaxSize; }

    /**
     * Returns current array size.
     *
     * @return size_t.
     */
    size_t Size() const { return mSize; }

    /**
     * Returns maximum available array size.
     *
     * @return size_t.
     */
    size_t MaxSize() const { return mMaxSize; }

    /**
     * Sets new array size.
     *
     * @param size new size.
     * @return Error.
     */
    Error Resize(size_t size, const T& value = T())
    {
        if (size > mMaxSize) {
            return ErrorEnum::eNoMemory;
        }

        if (size > mSize) {
            for (auto it = end(); it != end() + size - mSize; it++) {
                new (it) T(value);
            }
        }

        mSize = size;

        return ErrorEnum::eNone;
    }

    /**
     * Returns pointer to array items.
     *
     * @return T*.
     */
    T* Get(void) { return mItems; }

    /**
     * Returns pointer to const array items.
     *
     * @return const T*.
     */
    const T* Get(void) const { return mItems; }

    /**
     * Provides access to array item by index.
     *
     * @param index item index.
     * @return T&.
     */
    T& operator[](size_t index)
    {
        assert(index < mSize);

        return mItems[index];
    }

    /**
     * Provides access to array const item by index.
     *
     * @param index item index.
     * @return const T&.
     */
    const T& operator[](size_t index) const
    {
        assert(index < mSize);

        return mItems[index];
    }

    /**
     * Provides access to array item by index with boundaries check.
     *
     * @param index item index.
     * @return RetWithError<T&>.
     */
    RetWithError<T&> At(size_t index)
    {
        if (index >= mSize) {
            return {mItems[index], ErrorEnum::eOutOfRange};
        }

        return mItems[index];
    }

    /**
     * Provides access to array const item by index with boundaries check.
     *
     * @param index item index.
     * @return RetWithError<const T&>.
     */
    RetWithError<const T&> At(size_t index) const
    {
        if (index >= mSize) {
            return {mItems[index], ErrorEnum::eOutOfRange};
        }

        return mItems[index];
    }

    /**
     * Returns array first item.
     *
     * @return RetWithError<T&>.
     */
    RetWithError<T&> Front()
    {
        if (IsEmpty()) {
            return {mItems[0], ErrorEnum::eNotFound};
        }

        return mItems[0];
    }

    /**
     * Returns array const first item.
     *
     * @return RetWithError<const T&>.
     */
    RetWithError<const T&> Front() const
    {
        if (IsEmpty()) {
            return {mItems[0], ErrorEnum::eNotFound};
        }

        return mItems[0];
    }

    /**
     * Returns array last item.
     *
     * @return RetWithError<T&>.
     */
    RetWithError<T&> Back()
    {
        if (IsEmpty()) {
            return {mItems[0], ErrorEnum::eNotFound};
        }

        return mItems[mSize - 1];
    }

    /**
     * Returns array const last item.
     *
     * @return RetWithError<const T&>.
     */
    RetWithError<const T&> Back() const
    {
        if (IsEmpty()) {
            return {mItems[0], ErrorEnum::eNotFound};
        }

        return mItems[mSize - 1];
    }

    /**
     * Pushes item at the end of array.
     *
     * @param item item to push.
     * @return Error.
     */
    Error PushBack(const T& item)
    {
        if (IsFull()) {
            return ErrorEnum::eNoMemory;
        }

        new (const_cast<RemoveConstType<T>*>(end())) T(item);

        mSize++;

        return ErrorEnum::eNone;
    }

    /**
     * Creates item at the end of array.
     *
     * @param args args of item constructor.
     * @return Error.
     */
    template <typename... Args>
    Error EmplaceBack(Args&&... args)
    {
        if (IsFull()) {
            return ErrorEnum::eNoMemory;
        }

        new (end()) T(args...);

        mSize++;

        return ErrorEnum::eNone;
    }

    /**
     * Pops item from the end of array.
     *
     * @return Error.
     */
    Error PopBack()
    {
        if (IsEmpty()) {
            return ErrorEnum::eNotFound;
        }

        mSize--;

        return ErrorEnum::eNone;
    }

    /**
     * Returns true if array is not empty.
     *
     * @return bool.
     */
    operator bool() const { return Size() > 0; }

    /**
     * Checks if array equals to another array.
     *
     * @param array to compare with.
     * @return bool.
     */
    bool operator==(const Array& array) const
    {
        if (array.Size() != mSize) {
            return false;
        }

        for (size_t i = 0; i < mSize; i++) {
            if (!(mItems[i] == array.mItems[i])) {
                return false;
            }
        }

        return true;
    };

    /**
     * Checks if array doesn't equal to another array.
     *
     * @param array to compare with.
     * @return bool.
     */
    bool operator!=(const Array& array) const { return !operator==(array); };

    /**
     * Inserts items from range.
     *
     * @param pos insert position.
     * @param from insert from this position.
     * @param till insert till this position.
     * @return Error.
     */
    Error Insert(T* pos, const T* from, const T* till)
    {
        auto size = till - from;

        if (mSize + size > mMaxSize) {
            return ErrorEnum::eNoMemory;
        }

        if (pos < begin() || pos > end()) {
            return ErrorEnum::eInvalidArgument;
        }

        for (auto i = 0; i < end() - pos; i++) {
            new (pos + size + i) T(*(pos + i));
        }

        for (auto i = 0; i < size; i++) {
            new (pos + i) T(*(from + i));
        }

        mSize += size;

        return ErrorEnum::eNone;
    }

    /**
     * Appends array.
     *
     * @param array array to append with.
     * @return Array&.
     */
    Array& Append(const Array& array)
    {
        auto err = Insert(end(), array.begin(), array.end());
        assert(err.IsNone());
    }

    /**
     * Appends array operator.
     *
     * @param array array to append with.
     * @return Array&.
     */
    Array& operator+=(const Array& array) { return Append(array); }

    /**
     * Finds element in array.
     *
     * @param item element to find.
     * @return RetWithError<T*>.
     */
    RetWithError<T*> Find(const T& item) const
    {
        for (auto it = mItems; it != end(); it++) {
            if (*it == item) {
                return it;
            }
        }

        return {nullptr, ErrorEnum::eNotFound};
    }

    /**
     * Finds element in array that match argument.
     *
     * @param match match function.
     * @return RetWithError<T*>.
     */
    template <typename P>
    RetWithError<T*> Find(P match) const
    {
        for (auto it = mItems; it != end(); it++) {
            if (match(*it)) {
                return it;
            }
        }

        return {nullptr, ErrorEnum::eNotFound};
    }

    /**
     * Removes item from array.
     *
     * @param item item to remove.
     * @return RetWithError<T*> pointer to next after deleted item.
     */
    RetWithError<T*> Remove(T* item)
    {
        if (item < begin() || item >= end()) {
            return {nullptr, ErrorEnum::eInvalidArgument};
        }

        if (item == end() - 1) {
            (item)->~T();
        } else {
            for (auto i = 0; i < end() - item - 1; i++) {
                (item + i)->~T();
                new (item + i) T(*(item + i + 1));
                (item + i + 1)->~T();
            }
        }

        mSize--;

        return item;
    }

    /**
     * Removes element from array that match argument.
     *
     * @param match match function.
     * @return RetWithError<T*> pointer to end of new array.
     */
    template <typename P>
    RetWithError<T*> Remove(P match)
    {
        for (auto it = begin(); it != end();) {
            if (match(*it)) {
                auto result = Remove(it);
                if (!result.mError.IsNone()) {
                    return result;
                }

                it = result.mValue;
            } else {
                it++;
            }
        }

        return end();
    }

    /*
     * Sorts array items using sort function.
     *
     * @tparam F type of sort function.
     * @param sortFunc sort function.
     */
    template <typename F>
    void Sort(F sortFunc)
    {
        for (size_t i = 0; i < mSize - 1; i++) {
            for (size_t j = 0; j < mSize - i - 1; j++) {
                if (sortFunc(mItems[j], mItems[j + 1])) {
                    auto tmp = mItems[j + 1];
                    mItems[j + 1] = mItems[j];
                    mItems[j] = tmp;
                }
            }
        }
    }

    /**
     * Sorts array items using default comparision operator.
     */
    void Sort()
    {
        Sort([](const T& val1, const T& val2) { return val1 > val2; });
    }

    // Used for range based loop.
    T*       begin(void) { return &mItems[0]; }
    T*       end(void) { return &mItems[mSize]; }
    const T* begin(void) const { return &mItems[0]; }
    const T* end(void) const { return &mItems[mSize]; }

protected:
    void SetBuffer(const Buffer& buffer, size_t maxSize = 0)
    {
        if (maxSize == 0) {
            mMaxSize = buffer.Size() / sizeof(T);
        } else {
            mMaxSize = maxSize;
        }

        assert(mMaxSize != 0);

        mItems = static_cast<T*>(buffer.Get());
    }

    void SetSize(size_t size) { mSize = size; }

private:
    T*     mItems = nullptr;
    size_t mSize = 0;
    size_t mMaxSize = 0;
};

/**
 * Dynamic array instance.
 *
 * @tparam T type of items.
 * @tparam cMaxSize max size.
 */
template <typename T, size_t cMaxSize>
class DynamicArray : public Array<T> {
public:
    /**
     * Create dynamic array.
     */
    explicit DynamicArray()
        : mBuffer(cMaxSize * sizeof(T))
    {
        Array<T>::SetBuffer(mBuffer);
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates dynamic array from another array.
     *
     * @param array array to create from.
     */
    DynamicArray(const Array<T>& array)
    {
        Array<T>::SetBuffer(mBuffer);
        Array<T>::operator=(array);
    }

private:
    DynamicBuffer mBuffer;
};

/**
 * Static array instance.
 *
 * @tparam T type of items.
 * @tparam cMaxSize max size.
 */
template <typename T, size_t cMaxSize>
class StaticArray : public Array<T> {
public:
    /**
     * Creates static array.
     */
    StaticArray() { Array<T>::SetBuffer(mBuffer); }

    /**
     * Creates static array from another static array.
     *
     * @param array array to create from.
     */
    StaticArray(const StaticArray& array)
        : Array<T>()
    {
        Array<T>::SetBuffer(mBuffer);
        Array<T>::operator=(array);
    }

    /**
     * Assigns static array from another static array.
     *
     * @param array array to create from.
     */
    StaticArray& operator=(const StaticArray& array)
    {
        Array<T>::SetBuffer(mBuffer);
        Array<T>::operator=(array);

        return *this;
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates static array from another array.
     *
     * @param array array to create from.
     */
    StaticArray(const Array<T>& array)
    {
        Array<T>::SetBuffer(mBuffer);
        Array<T>::operator=(array);
    }

private:
    StaticBuffer<cMaxSize * sizeof(T)> mBuffer;
};

} // namespace aos

#endif
