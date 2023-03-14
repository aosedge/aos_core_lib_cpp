/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ARRAY_HPP_
#define AOS_ARRAY_HPP_

#include <assert.h>

#include "aos/common/buffer.hpp"
#include "aos/common/error.hpp"

namespace aos {

/**
 * Array instance.
 * @tparam T array type.
 * @tparam cMaxSize max array size.
 */
template <typename T>
class Array {
public:
    /**
     * Crates empty array instance.
     */
    Array()
        : mItems(nullptr)
        , mSize(0)
        , mMaxSize(0)
    {
    }

    /**
     * Crates array instance over the buffer.
     *
     * @param buffer underlying buffer.
     * @param size current array size.
     */
    explicit Array(const Buffer& buffer, size_t size = 0)
        : mSize(size)
    {
        SetBuffer(buffer, size);
    }

    /**
     * Creates array instance from C array.
     *
     * @param items const C array.
     * @param size C array size.
     */
    Array(T* items, size_t size)
        : mItems(items)
        , mSize(size)
        , mMaxSize(size)
    {
    }

    /**
     * Creates array instance from another array.
     *
     * @param array another array instance.
     */
    Array(const Array& array)
        : mItems(array.mItems)
        , mSize(array.mSize)
        , mMaxSize(array.mMaxSize)
    {
    }

    /**
     * Assigns existing array to the current one.
     *
     * @param array existing array.
     * @return Array&.
     */
    Array& operator=(const Array& array)
    {
        assert(array.mMaxSize <= mMaxSize);

        mSize = array.mSize;

        if (mItems == array.mItems) {
            return *this;
        }

        memcpy(mItems, array.mItems, array.mSize * sizeof(T));

        return *this;
    }

    /**
     * Clears array.
     */
    void Clear() { mSize = 0; }

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
    Error Resize(size_t size)
    {
        if (size > mMaxSize) {
            return ErrorEnum::eNoMemory;
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
     * @return RetWithError<T*>.
     */
    RetWithError<T*> At(size_t index)
    {
        if (index >= mSize) {
            return RetWithError<T*>(nullptr, ErrorEnum::eOutOfRange);
        }

        return RetWithError<T*>(&mItems[index], ErrorEnum::eNone);
    }

    /**
     * Provides access to array const item by index with boundaries check.
     *
     * @param index item index.
     * @return RetWithError<const T*>.
     */
    RetWithError<const T*> At(size_t index) const
    {
        if (index >= mSize) {
            return RetWithError<const T*>(nullptr, ErrorEnum::eOutOfRange);
        }

        return RetWithError<const T*>(&mItems[index], ErrorEnum::eNone);
    }

    /**
     * Returns array first item.
     *
     * @return RetWithError<T*>.
     */
    RetWithError<T*> Front() { return At(0); }

    /**
     * Returns array const first item.
     *
     * @return RetWithError<const T*>.
     */
    RetWithError<const T*> Front() const { return At(0); }

    /**
     * Returns array last item.
     *
     * @return RetWithError<T*>.
     */
    RetWithError<T*> Back() { return At(mSize - 1); }

    /**
     * Returns array const last item.
     *
     * @return RetWithError<const T*>.
     */
    RetWithError<const T*> Back() const { return At(mSize - 1); }

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

        mItems[mSize++] = item;

        return ErrorEnum::eNone;
    }

    /**
     * Pops item from the end of array.
     *
     * @return RetWithError<T*>..
     */
    RetWithError<T*> PopBack()
    {
        if (IsEmpty()) {
            RetWithError<T*>(nullptr, ErrorEnum::eNotFound);
        }

        mSize--;

        return RetWithError<T*>(&mItems[mSize], ErrorEnum::eNone);
    }

    // Used for range based loop.
    T*       begin(void) { return &mItems[0]; }
    T*       end(void) { return &mItems[mSize]; }
    const T* begin(void) const { return &mItems[0]; }
    const T* end(void) const { return &mItems[mSize]; }

protected:
    void SetBuffer(const Buffer& buffer, size_t size = 0)
    {
        mMaxSize = buffer.Size() / sizeof(T);
        mSize = size;

        assert(mMaxSize != 0);
        assert(mSize <= mMaxSize);

        mItems = static_cast<T*>(buffer.Get());
    }

private:
    T*     mItems;
    size_t mSize;
    size_t mMaxSize;
};

template <typename T, size_t cMaxSize>
class StaticArray : public Array<T> {
public:
    explicit StaticArray(size_t size = 0) { Array<T>::SetBuffer(mBuffer, size); }

private:
    StaticBuffer<cMaxSize * sizeof(T)> mBuffer;
};

template <typename T, size_t cMaxSize>
class DynamicArray : public Array<T> {
public:
    explicit DynamicArray(size_t size = 0)
        : mBuffer(cMaxSize * sizeof(T))
    {
        Array<T>::SetBuffer(mBuffer, size);
    }

private:
    DynamicBuffer mBuffer;
};

} // namespace aos

#endif
