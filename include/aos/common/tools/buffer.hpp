/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_BUFFER_HPP_
#define AOS_BUFFER_HPP_

#include <assert.h>
#include <cstdint>
#include <string.h>

namespace aos {

/**
 * Buffer instance.
 */
class Buffer {
public:
    /**
     * Copies one buffer to another.
     *
     * @param buffer to copy from.
     * @return Buffer&.
     */
    Buffer& operator=(const Buffer& buffer)
    {
        assert(mSize >= buffer.mSize);

        memcpy(mBuffer, buffer.mBuffer, buffer.mSize);

        return *this;
    }

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

    void SetBuffer(void* buffer, size_t size)
    {
        mBuffer = buffer;
        mSize = size;
    }

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
    explicit DynamicBuffer(size_t size) { SetBuffer(operator new(size), size); }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs dynamic buffer from another buffer.
     *
     * @param buffer buffer to crate from.
     */
    DynamicBuffer(const Buffer& buffer)
    {
        SetBuffer(operator new(buffer.Size()), buffer.Size());
        Buffer::operator=(buffer);
    }

    /**
     * Copies one buffer to another.
     *
     * @param buffer to copy from.
     * @return Buffer&.
     */
    DynamicBuffer& operator=(const Buffer& buffer)
    {
        Buffer::operator=(buffer);

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
    StaticBuffer() { SetBuffer(mBuffer, cSize); }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs static buffer from another buffer.
     *
     * @param buffer buffer to crate from.
     */
    StaticBuffer(const Buffer& buffer)
    {
        SetBuffer(mBuffer, cSize);
        Buffer::operator=(buffer);
    }

    /**
     * Copies one buffer to another.
     *
     * @param buffer to copy from.
     * @return Buffer&.
     */
    StaticBuffer& operator=(const Buffer& buffer)
    {
        Buffer::operator=(buffer);

        return *this;
    }

private:
    uint8_t mBuffer[cSize];
};

} // namespace aos

#endif
