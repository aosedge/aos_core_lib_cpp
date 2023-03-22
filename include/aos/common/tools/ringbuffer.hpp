/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_RINGBUFFER_HPP_
#define AOS_RINGBUFFER_HPP_

#include "aos/common/tools/error.hpp"

namespace aos {

/**
 * Linear ring buffer class.
 *
 * @tparam cBufferSize
 */
template <size_t cBufferSize>
class LinearRingBuffer {
public:
    /**
     * Creates linear ring buffer instance.
     */
    LinearRingBuffer()
        : mBuffer()
        , mHead(mBuffer)
        , mTail(mBuffer)
        , mBufferEnd(mBuffer + cBufferSize)
        , mSize(0)
    {
    }

    /**
     * Pushes data from external buffer.
     *
     * @param data pinter to external buffer.
     * @param size external buffer size.
     * @return Error.
     */
    Error Push(const void* data, size_t size)
    {
        if (size > mBufferEnd - mBuffer - mSize
            || ((mTail + size > mBufferEnd) && (size > static_cast<size_t>(mHead - mBuffer)))) {
            return ErrorEnum::eNoMemory;
        }

        // handle end of the buffer case
        if (mTail + size > mBufferEnd) {
            mBufferEnd = mTail;
            mTail = mBuffer;

            if (mSize == 0) {
                mHead = mBuffer;
            }
        }

        memcpy(mTail, data, size);

        mTail += size;
        mSize += size;

        if (mTail == mBufferEnd) {
            mTail = mBuffer;
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

        memcpy(data, mHead, size);

        mHead += size;
        mSize -= size;

        if (mHead > mBufferEnd) {
            return ErrorEnum::eOutOfRange;
        }

        if (mHead == mBufferEnd) {
            mHead = mBuffer;
            mBufferEnd = mBuffer + cBufferSize;
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
     * Returns point to buffer head.

     * @return void*.
     */
    void* Head() const { return mHead; }

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
    size_t MaxSize() const { return cBufferSize; };

    /**
     * Clears buffer.
     */
    void Clear()
    {
        mHead = mTail = mBuffer;
        mSize = 0;
    }

private:
    uint8_t  mBuffer[cBufferSize];
    uint8_t* mHead;
    uint8_t* mTail;
    uint8_t* mBufferEnd;
    size_t   mSize;
};

} // namespace aos

#endif
