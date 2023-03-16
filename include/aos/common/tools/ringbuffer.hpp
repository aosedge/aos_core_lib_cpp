/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_RINGBUFFER_HPP_
#define AOS_RINGBUFFER_HPP_

#include <assert.h>
#include <cstdint>

#include "aos/common/tools/buffer.hpp"
#include "aos/common/tools/error.hpp"
#include "aos/common/tools/noncopyable.hpp"

namespace aos {

/**
 * Ring buffer instance.
 */
class RingBuffer {
public:
    /**
     * Creates ring buffer instance.
     */
    RingBuffer() = default;

    /**
     * Pushes data from external buffer.
     *
     * @param data pinter to external buffer.
     * @param size external buffer size.
     * @return Error.
     */
    Error Push(const void* data, size_t size)
    {
        if (size > MaxSize() - mSize) {
            return ErrorEnum::eNoMemory;
        }

        size_t copiedSize = 0;

        // handle end of the buffer case
        if (mTail + size > mEnd) {
            copiedSize = mEnd - mTail;
            memcpy(mTail, data, copiedSize);
            mTail = mBegin;
        }

        memcpy(mTail, &(static_cast<const uint8_t*>(data))[copiedSize], size - copiedSize);
        mSize += size;
        mTail += size - copiedSize;

        if (mTail == mEnd) {
            mTail = mBegin;
        }

        return ErrorEnum::eNone;
    }

    /**
     * Pops data to external buffer.
     * @param data pinter to external buffer.
     * @param size external buffer size.
     * @return Error.
     */
    Error Pop(void* data, size_t size)
    {
        if (size > mSize) {
            return ErrorEnum::eInvalidArgument;
        }

        size_t copiedSize = 0;

        // handle end of the buffer case
        if (mHead + size > mEnd) {
            copiedSize = mEnd - mHead;

            memcpy(data, mHead, copiedSize);

            mHead = mBegin;
        }

        memcpy(&(static_cast<uint8_t*>(data))[copiedSize], mHead, size - copiedSize);
        mSize -= size;
        mHead += size - copiedSize;

        if (mHead == mEnd) {
            mHead = mBegin;
        }

        return ErrorEnum::eNone;
    }

    /**
     * Pushes value to buffer.
     *
     * @tparam T data type.
     * @param value value reference.
     * @return Error.
     */
    template <typename T>
    Error PushValue(const T& value)
    {
        return Push(&value, sizeof(T));
    }

    /**
     * Pops value from buffer.
     *
     * @tparam T data type.
     * @return T value.
     */
    template <typename T>
    T PopValue()
    {
        T tmp;

        auto err = Pop(&tmp);
        assert(err.IsNone());

        return tmp;
    }

    /**
     * Pushes data to buffer.
     *
     * @tparam T data type.
     * @param data pointer to data buffer.
     * @return Error.
     */
    template <typename T>
    Error Push(const T* data)
    {
        return Push(data, sizeof(T));
    }

    /**
     * Pops from buffer.
     *
     * @tparam T data type.
     * @param data pointer to data buffer.
     * @return Error.
     */
    template <typename T>
    Error Pop(T* data)
    {
        return Pop(data, sizeof(T));
    }

    /**
     * Checks if buffer is empty.
     *
     * @return bool.
     */
    bool IsEmpty() const { return mSize == 0; }

    /**
     * Returns current buffer size.
     *
     * @return size_t.
     */
    size_t Size() const { return mSize; }

    /**
     * Returns max buffer size.
     *
     * @return size_t.
     */
    size_t MaxSize() const { return mEnd - mBegin; };

    /**
     * Clears buffer.
     */
    void Clear()
    {
        mHead = mTail = mBegin;
        mSize = 0;
    }

protected:
    void SetBuffer(Buffer& buffer)
    {
        mBegin = static_cast<uint8_t*>(buffer.Get());
        mEnd = mBegin + buffer.Size();
        mHead = mTail = mBegin;
        mSize = 0;
    }

private:
    uint8_t* mBegin {};
    uint8_t* mEnd {};
    uint8_t* mHead {};
    uint8_t* mTail {};
    size_t   mSize {};
};

/**
 * Static ring buffer.
 *
 * @tparam cMaxSize max ring buffer size.
 */
template <size_t cMaxSize>
class StaticRingBuffer : public RingBuffer, private NonCopyable {
public:
    /**
     * Creates static ring buffer.
     */
    StaticRingBuffer() { SetBuffer(mBuffer); }

private:
    StaticBuffer<cMaxSize> mBuffer;
};

} // namespace aos

#endif
