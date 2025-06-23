/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ARRAY_HPP_
#define AOS_ARRAY_HPP_

#include <assert.h>

#include "aos/common/tools/algorithm.hpp"
#include "aos/common/tools/buffer.hpp"
#include "aos/common/tools/new.hpp"

namespace aos {

/**
 * Array instance.
 * @tparam T array type.
 */
template <typename T>
class Array : public AlgorithmItf<T, T*, const T*> {
public:
    using Iterator      = T*;
    using ConstIterator = const T*;

    /**
     * Crates empty array instance.
     */
    Array() = default;

    /**
     * Crates array instance over the buffer.
     *
     * @param buffer underlying buffer.
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
        [[maybe_unused]] auto err = Assign(array);
        assert(err.IsNone());

        return *this;
    }

    /**
     * Assigns existing array to the current one.
     *
     * @param array existing array.
     * @return Error.
     */
    Error Assign(const Array& array)
    {
        if (this == &array) {
            return ErrorEnum::eNone;
        }

        if (!mItems || array.Size() > mMaxSize) {
            return ErrorEnum::eNoMemory;
        }

        Clear();

        mSize = array.mSize;

        for (size_t i = 0; i < array.Size(); i++) {
            new (&mItems[i]) T(array.mItems[i]);
        }

        return ErrorEnum::eNone;
    }

    /**
     * Rebinds internal buffer to another array buffer.
     *
     * @param array another array instance.
     */
    void Rebind(const Array& array)
    {
        Clear();

        mItems   = array.mItems;
        mSize    = array.mSize;
        mMaxSize = array.mMaxSize;
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
     * Returns current array size.
     *
     * @return size_t.
     */
    size_t Size() const override { return mSize; }

    /**
     * Returns maximum available array size.
     *
     * @return size_t.
     */
    size_t MaxSize() const override { return mMaxSize; }

    /**
     * Sets new array size.
     *
     * @param size new size.
     * @return Error.
     */
    Error Resize(size_t size)
    {
        if (size > mMaxSize) {
            return ErrorEnum::eNoMemory;
        }

        if (size > mSize) {
            for (auto it = end(); it != end() + size - mSize; it++) {
                new (it) T();
            }
        }

        mSize = size;

        return ErrorEnum::eNone;
    }

    /**
     * Sets new array size and fills it with value.
     *
     * @param size new size.
     * @param value value to fill.
     * @return Error.
     */
    Error Resize(size_t size, const T& value)
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
     * Pushes item at the end of array.
     *
     * @param item item to push.
     * @return Error.
     */
    Error PushBack(const T& item)
    {
        if (mSize == mMaxSize) {
            return ErrorEnum::eNoMemory;
        }

        new (const_cast<RemoveConstType<T>*>(end())) T(item);

        mSize++;

        return ErrorEnum::eNone;
    }

    /**
     * Pushes item at the end of array.
     *
     * @param item item to push.
     * @return Error.
     */
    Error PushBack(T&& item)
    {
        if (mSize == mMaxSize) {
            return ErrorEnum::eNoMemory;
        }

        new (const_cast<RemoveConstType<T>*>(end())) T(Move(item));

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
        if (mSize == mMaxSize) {
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
        if (mSize == 0) {
            return ErrorEnum::eNotFound;
        }

        mSize--;

        return ErrorEnum::eNone;
    }

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
        const size_t size = till - from;

        if (mSize + size > mMaxSize) {
            return ErrorEnum::eNoMemory;
        }

        if (pos < begin() || pos > end()) {
            return ErrorEnum::eInvalidArgument;
        }

        for (auto i = end() - pos - 1; i >= 0; i--) {
            new (pos + size + i) T(*(pos + i));
        }

        for (size_t i = 0; i < size; i++) {
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
        [[maybe_unused]] auto err = Insert(end(), array.begin(), array.end());
        assert(err.IsNone());

        return *this;
    }

    /**
     * Appends array operator.
     *
     * @param array array to append with.
     * @return Array&.
     */
    Array& operator+=(const Array& array) { return Append(array); }

    /**
     * Erases items range from array.
     *
     * @param first first item to erase.
     * @param first last item to erase.
     * @return next after deleted item iterator.
     */
    Iterator Erase(ConstIterator first, ConstIterator last) override
    {
        if (first < begin() || first >= end() || last < begin() || last > end()) {
            assert(false);
        }

        auto curEnd = end();

        for (auto it = first; it != last; ++it) {
            it->~T();
            mSize--;
        }

        auto curFirst = first;

        for (T* it = const_cast<RemoveConstType<T>*>(last); it != curEnd; ++it, ++curFirst) {
            new (const_cast<RemoveConstType<T>*>(curFirst)) T(Move(*it));
            it->~T();
        }

        return const_cast<Iterator>(first);
    }

    /**
     * Erases item from array.
     *
     * @param it item to erase.
     * @return next after deleted item iterator.
     */
    Iterator Erase(ConstIterator it) override
    {
        auto next = it;

        return Erase(it, ++next);
    }

    // Used for range based loop.
    T*       begin(void) override { return &mItems[0]; }
    T*       end(void) override { return &mItems[mSize]; }
    const T* begin(void) const override { return &mItems[0]; }
    const T* end(void) const override { return &mItems[mSize]; }

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
    T*     mItems   = nullptr;
    size_t mSize    = 0;
    size_t mMaxSize = 0;
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
     * Creates static array with fixed size.
     *
     * @param size fixed size.
     */
    explicit StaticArray(size_t size)
    {
        Array<T>::SetBuffer(mBuffer);
        [[maybe_unused]] auto err = Array<T>::Resize(size);
        assert(err.IsNone());
    }

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
     * Destroys static array.
     */
    ~StaticArray() { Array<T>::Clear(); }

    /**
     * Assigns static array from another static array.
     *
     * @param array array to create from.
     */
    StaticArray& operator=(const StaticArray& array)
    {
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

    /**
     * Assigns static array from another  array.
     *
     * @param array array to assign from.
     */
    StaticArray& operator=(const Array<T>& array)
    {
        Array<T>::operator=(array);

        return *this;
    }

private:
    StaticBuffer<cMaxSize * sizeof(T)> mBuffer;
};

} // namespace aos

#endif
