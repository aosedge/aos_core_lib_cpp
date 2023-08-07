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

#include "aos/common/tools/utils.hpp"

namespace aos {

/**
 * Buffer instance.
 */
class Buffer {
public:
    /**
     * Creates buffer from existing memory region.
     *
     * @param buffer points to existing memory region.
     * @param size region size.
     */
    Buffer(const void* buffer, size_t size)
        : mBuffer(const_cast<RemoveConstType<void>*>(buffer))
        , mSize(size)
    {
    }

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
     * Checks if buffer equals to another buffer.
     *
     * @param buffer buffer to compare with.
     * @return bool.
     */
    bool operator==(const Buffer& buffer) const
    {
        if (mSize != buffer.Size()) {
            return false;
        }

        return memcmp(mBuffer, buffer.Get(), mSize) == 0;
    };

    /**
     * Checks if buffer equals to another buffer.
     *
     * @param buffer buffer to compare with.
     * @return bool.
     */
    bool operator!=(const Buffer& buffer) const { return !operator==(buffer); };

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
    explicit DynamicBuffer(size_t size)
        : Buffer(operator new(size), size)
    {
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs dynamic buffer from another buffer.
     *
     * @param buffer buffer to crate from.
     */
    DynamicBuffer(const Buffer& buffer)
        : Buffer(operator new(buffer.Size()), buffer.Size())
    {
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
