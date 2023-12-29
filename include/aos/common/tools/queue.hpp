/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_QUEUE_HPP_
#define AOS_QUEUE_HPP_

#include <assert.h>

#include "aos/common/tools/buffer.hpp"
#include "aos/common/tools/error.hpp"
#include "aos/common/tools/new.hpp"

namespace aos {

/**
 * Queue instance.
 *
 * @tparam T
 */
template <typename T>
class Queue {
public:
    /**
     * Creates queue.
     */
    Queue() = default;

    /**
     * Crates queue from buffer.
     *
     * @param buffer buffer to create queue.
     */
    explicit Queue(const Buffer& buffer) { SetBuffer(buffer); }

    /**
     * Creates queue instance from C array.
     *
     * @param items const C array.
     * @param size C array size.
     */
    Queue(T* items, size_t size)
        : mBegin(items)
        , mEnd(mBegin + size)
        , mHead(mBegin)
        , mTail(mBegin)
        , mSize(size)
        , mMaxSize(size)
    {
    }

    /**
     * Creates queue from another queue.
     *
     * @param queue queue to create from.
     */
    Queue(const Queue& queue) = default;

    /**
     * Copy queue from another queue.
     *
     * @param queue gueue to copy from.
     *
     * @return Queue&.
     */
    Queue& operator=(const Queue& queue)
    {
        assert(mBegin && queue.mSize <= mMaxSize);

        if (mBegin == queue.mBegin) {
            mHead = queue.mHead;
            mTail = queue.mTail;
            mSize = queue.mSize;

            return *this;
        }

        mSize = 0;
        mHead = mTail = mBegin;

        auto it = queue.mHead;

        while (mSize < queue.mSize) {
            new (mTail) T(*it);

            it++;
            if (it == queue.mEnd) {
                it = queue.mBegin;
            }

            mTail++;

            if (mTail == mEnd) {
                mTail = mBegin;
            }

            mSize++;
        }

        return *this;
    }

    /**
     * Pushes item to queue.
     *
     * @param item item to push.
     * @return Error.
     */
    Error Push(const T& item)
    {
        if (mSize >= mMaxSize) {
            return ErrorEnum::eNoMemory;
        }

        new (mTail) T(item);
        mSize++;
        mTail++;

        if (mTail == mEnd) {
            mTail = mBegin;
        }

        return ErrorEnum::eNone;
    }

    /**
     * Pops front item from queue.
     *
     * @return Error.
     */
    Error Pop()
    {
        if (IsEmpty()) {
            return ErrorEnum::eNotFound;
        }

        mHead++;
        mSize--;

        if (mHead == mEnd) {
            mHead = mBegin;
        }

        return ErrorEnum::eNone;
    }

    /**
     * Returns front item.
     *
     * @return RetWithError<const T&> front item.
     */
    RetWithError<const T&> Front() const
    {
        if (IsEmpty()) {
            return {*mHead, ErrorEnum::eNotFound};
        }

        return *mHead;
    }

    /**
     * Returns front item.
     *
     * @return RetWithError<T&> front item.
     */
    RetWithError<T&> Front()
    {
        if (IsEmpty()) {
            return {*mHead, ErrorEnum::eNotFound};
        }

        return *mHead;
    }

    /**
     * Returns back item.
     *
     * @return RetWithError<T&> back item.
     */
    RetWithError<T&> Back()
    {
        auto it = LastItem();

        if (IsEmpty()) {
            return {*it, ErrorEnum::eNotFound};
        }

        return *it;
    }

    /**
     * Returns back item.
     *
     * @return RetWithError<const T&> back item.
     */
    RetWithError<const T&> Back() const
    {
        auto it = LastItem();

        if (IsEmpty()) {
            return {*it, ErrorEnum::eNotFound};
        }

        return *it;
    }

    /**
     * Return current queue size.
     *
     * @return size_t.
     */
    size_t Size() const { return mSize; }

    /**
     * Return queue max size.
     *
     * @return size_t
     */
    size_t MaxSize() const { return mMaxSize; }

    /**
     * Clears queue.
     */
    void Clear()
    {
        while (mSize) {
            Back().mValue.~T();
            Pop();
        }

        mHead = mTail = mBegin;
        mSize         = 0;
    }

    /**
     * Checks if queue is empty.
     *
     * @return bool.
     */
    bool IsEmpty() const { return mSize == 0; }

    /**
     * Checks if queue is full.
     *
     * @return bool.
     */
    bool IsFull() const { return mSize == mMaxSize; }

protected:
    void SetBuffer(const Buffer& buffer)
    {
        mMaxSize = buffer.Size() / sizeof(T);

        assert(mMaxSize != 0);

        mBegin = static_cast<T*>(buffer.Get());
        mEnd   = mBegin + mMaxSize;

        Clear();
    }

private:
    T* LastItem()
    {
        auto it = mTail - 1;

        if (it < mBegin) {
            it = mEnd - 1;
        }

        return it;
    }

    T*     mBegin   = nullptr;
    T*     mEnd     = nullptr;
    T*     mHead    = nullptr;
    T*     mTail    = nullptr;
    size_t mSize    = 0;
    size_t mMaxSize = 0;
};

/**
 * Dynamic queue instance.
 *
 * @tparam T type of items.
 * @tparam cMaxSize max size.
 */
template <typename T, size_t cMaxSize>
class DynamicQueue : public Queue<T> {
public:
    /**
     * Create dynamic queue.
     */
    explicit DynamicQueue()
        : mBuffer(cMaxSize * sizeof(T))
    {
        Queue<T>::SetBuffer(mBuffer);
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates dynamic queue from another queue.
     *
     * @param queue queue to create from.
     */
    DynamicQueue(const Queue<T>& queue)
    {
        Queue<T>::SetBuffer(mBuffer);
        Queue<T>::operator=(queue);
    }

private:
    DynamicBuffer mBuffer;
};

/**
 * Static queue instance.
 *
 * @tparam T type of items.
 * @tparam cMaxSize max size.
 */
template <typename T, size_t cMaxSize>
class StaticQueue : public Queue<T> {
public:
    /**
     * Creates static queue.
     */
    explicit StaticQueue() { Queue<T>::SetBuffer(mBuffer); }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates static queue from another queue.
     *
     * @param queue queue to create from.
     */
    StaticQueue(const Queue<T>& queue)
    {
        Queue<T>::SetBuffer(mBuffer);
        Queue<T>::operator=(queue);
    }

private:
    StaticBuffer<cMaxSize * sizeof(T)> mBuffer;
};

} // namespace aos

#endif
