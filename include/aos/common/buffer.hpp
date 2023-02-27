/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_BUFFER_HPP_
#define AOS_BUFFER_HPP_

#include <cstdint>
#include <string.h>

namespace aos {

/**
 * Buffer instance.
 */
class Buffer {
public:
    /**
     * Returns pointer to the hold buffer.
     *
     * @return void*.
     */
    void* Get() const { return mBuffer; }

    /**
     * Returns buffer size.
     *
     * @return size_t.
     */
    size_t Size() const { return mSize; }

protected:
    Buffer()
        : mBuffer(nullptr)
        , mSize(0)
    {
    }

    Buffer(void* buffer, size_t size)
        : mBuffer(buffer)
        , mSize(size)
    {
    }

    void Resize(size_t size) { mSize = size; }
    void SetBuffer(void* buffer) { mBuffer = buffer; }

private:
    void*  mBuffer;
    size_t mSize;
};

/**
 * Dynamic buffer instance.
 */
class DynamicBuffer : public Buffer {
public:
    /**
     * Creates dynamic buffer instance with specified size.
     *
     * @param size buffer size.
     */
    explicit DynamicBuffer(size_t size)
        : Buffer(operator new(size), size)
    {
    }

    /**
     * Creates dynamic buffer from existing instance.
     *
     * @param buffer existing dynamic buffer instance.
     */
    DynamicBuffer(const DynamicBuffer& buffer)
        : Buffer(operator new(buffer.Size()), buffer.Size())
    {
        memcpy(Get(), buffer.Get(), Size());
    }

    /**
     * Assigns existing buffer instance to the current one.
     *
     * @param buffer existing dynamic buffer instance.
     * @return DynamicBuffer&.
     */
    DynamicBuffer& operator=(const DynamicBuffer& buffer)
    {
        if (&buffer == this) {
            return *this;
        }

        operator delete(Get());

        SetBuffer(operator new(buffer.Size()));
        Resize(buffer.Size());

        memcpy(Get(), buffer.Get(), Size());

        return *this;
    }

    ~DynamicBuffer() { operator delete(Get()); }
};

/**
 * Static buffer instance.
 */
template <size_t cSize>
class StaticBuffer : public Buffer {
public:
    /**
     * Creates static buffer instance with fixed size.
     */
    StaticBuffer()
    {
        SetBuffer(mBuffer);
        Resize(cSize);
    }

private:
    uint8_t mBuffer[cSize];
};

} // namespace aos

#endif
