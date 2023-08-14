/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ALLOCATOR_HPP_
#define AOS_ALLOCATOR_HPP_

#include <assert.h>
#include <cstdint>

#include "aos/common/tools/array.hpp"
#include "aos/common/tools/buffer.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/tools/thread.hpp"

namespace aos {

/**
 * Allocator instance.
 */
class Allocator : private NonCopyable {
public:
    /**
     * Allocation instance.
     */
    class Allocation {
    public:
        /**
         * Creates allocation.
         *
         * @param data pointer to allocated data.
         * @param size allocated size.
         */
        Allocation(uint8_t* data, size_t size)
            : mData(data)
            , mSize(size)
            , mSharedCount(0)
        {
        }

        /**
         * Returns pointer to allocated data.
         *
         * @return uint8_t* pointer to allocated data.
         */
        uint8_t* Data() const { return mData; }

        /**
         * Returns allocated size.
         *
         * @return size_t allocated size.
         */
        size_t Size() const { return mSize; }

        /**
         * Increases shared count.
         *
         * @param mutex mutex to lock context.
         * @return size_t chared count value;
         */
        size_t Take(Mutex& mutex)
        {
            LockGuard lock(mutex);

            return ++mSharedCount;
        }

        /**
         * Decreases shared count.
         *
         * @param mutex mutex to lock context.
         * @return size_t chared count value;
         */
        size_t Give(Mutex& mutex)
        {
            LockGuard lock(mutex);

            return --mSharedCount;
        }

    private:
        uint8_t* mData;
        size_t   mSize;
        size_t   mSharedCount;
    };

    /**
     * Clears allocator.
     */
    void Clear()
    {
        LockGuard lock(mMutex);

        mAllocations->Clear();
    }

    /**
     * Allocates data with specified size.
     *
     * @param size allocate size.
     * @return void* pointer to allocated data.
     */
    void* Allocate(size_t size)
    {
        LockGuard lock(mMutex);

        if (mAllocations->IsFull() || GetAllocatedSize() >= mMaxSize) {
            assert(false);
            return nullptr;
        }

        auto data = End();

        mAllocations->EmplaceBack(data, size);

        return data;
    }

    /**
     * Frees previously allocated data.
     *
     * @param data allocated data to free.
     */
    void Free(void* data)
    {
        LockGuard lock(mMutex);

        auto curEnd = mAllocations->end();
        auto newEnd
            = mAllocations->Remove([data](const Allocation& allocation) { return allocation.Data() == data; }).mValue;

        assert(newEnd && curEnd != newEnd);
    }

    /**
     * Finds allocation by data.
     *
     * @param data allocated data.
     * @return void* pointer to allocation.
     */
    RetWithError<Allocation*> FindAllocation(const void* data)
    {
        LockGuard lock(mMutex);

        return mAllocations->Find([data](const Allocation& allocation) { return allocation.Data() == data; });
    }

    /**
     * Increases allocation shared count.
     *
     * @param allocation allocation to increase shared count.
     * @return size_t allocation shared count.
     */
    size_t TakeAllocation(Allocation* allocation)
    {
        assert(allocation);

        return allocation->Take(mMutex);
    }

    /**
     * Decreases allocation shared count.
     *
     * @param allocation allocation to increase shared count.
     * @return size_t allocation shared count.
     */
    size_t GiveAllocation(Allocation* allocation)
    {
        assert(allocation);

        return allocation->Give(mMutex);
    }

    /**
     * Returns allocator free size.
     *
     * @return size_t free size.
     */
    size_t FreeSize()
    {
        LockGuard lock(mMutex);

        return mMaxSize - GetAllocatedSize();
    }

    /**
     * Return allocator max size.
     *
     * @return size_t max size.
     */
    size_t MaxSize()
    {
        LockGuard lock(mMutex);

        return mMaxSize;
    }

protected:
    void SetBuffer(const Buffer& buffer, Array<Allocation>& allocations)
    {
        mBuffer = static_cast<uint8_t*>(buffer.Get());
        mMaxSize = buffer.Size();
        mAllocations = &allocations;
        mAllocations->Clear();
    }

private:
    size_t GetAllocatedSize() const { return End() - mBuffer; }

    uint8_t* End() const
    {
        if (mAllocations->IsEmpty()) {
            return mBuffer;
        }

        auto const& lastAllocation = mAllocations->Back().mValue;

        return static_cast<uint8_t*>(lastAllocation.Data()) + lastAllocation.Size();
    }

    uint8_t*           mBuffer = {};
    Array<Allocation>* mAllocations = {};
    size_t             mMaxSize = {};
    Mutex              mMutex;
};

/**
 * Buffer allocator instance.
 */
template <size_t cNumAllocations = 8>
class BufferAllocator : public Allocator {
public:
    /**
     * Creates buffer allocator instance.
     */
    explicit BufferAllocator(const Buffer& buffer) { SetBuffer(buffer, mAllocations); }

private:
    StaticArray<Allocator::Allocation, cNumAllocations> mAllocations;
};

/**
 * Static allocator instance.
 */
template <size_t cSize, size_t cNumAllocations = 8>
class StaticAllocator : public Allocator {
public:
    /**
     * Creates static allocator instance.
     */
    StaticAllocator() { SetBuffer(mBuffer, mAllocations); }

private:
    StaticBuffer<cSize>                                 mBuffer;
    StaticArray<Allocator::Allocation, cNumAllocations> mAllocations;
};

} // namespace aos

/**
 * Overloads placement new operator to allocate object on Aos buffer.
 *
 * @param size allocate size.
 * @param allocator allocator.
 * @return void* allocated space.
 */
inline void* operator new(size_t size, aos::Allocator* allocator)
{
    assert(allocator);

    auto data = allocator->Allocate(size);
    assert(data);

    return data;
}

/**
 * Overloads placement sized new operator to allocate object on Aos buffer.
 *
 * @param size allocate size.
 * @param allocator allocator.
 * @return void* allocated space.
 */
inline void* operator new[](size_t size, aos::Allocator* allocator)
{
    assert(allocator);

    auto data = allocator->Allocate(size);
    assert(data);

    return data;
}

/**
 * Overloads placement delete operator to release object on Aos buffer.
 *
 * @param data pointer to allocated data.
 * @param allocator allocator.
 */
inline void operator delete(void* data, aos::Allocator* allocator)
{
    assert(allocator);

    allocator->Free(data);
}

#endif
