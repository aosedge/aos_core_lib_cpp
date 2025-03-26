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

#include "aos/common/tools/buffer.hpp"
#include "aos/common/tools/list.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/tools/thread.hpp"

namespace aos {

/**
 * Allocator instance.
 */
class Allocator {
public:
    /**
     * Allocation instance.
     */
    class Allocation {
    public:
        /**
         * Creates allocation.
         */
        Allocation() = default;

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
        uint8_t* mData        = nullptr;
        size_t   mSize        = 0;
        size_t   mSharedCount = 0;
    };

    /**
     * Clears allocator.
     */
    void Clear()
    {
        LockGuard lock {mMutex};

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
        LockGuard lock {mMutex};

        if (mAllocations->IsFull() || GetAllocatedSize() + size > mMaxSize) {
            assert(false);
            return nullptr;
        }

        auto* pos = mBuffer;
        auto  it  = mAllocations->begin();

        for (; it != mAllocations->end(); ++it) {
            size_t availableSize = it->Data() - pos;

            if (availableSize >= size) {
                return Allocate(it, pos, size);
            }

            pos = it->Data() + it->Size();
        }

        if (pos + size <= mBuffer + mMaxSize) {
            return Allocate(mAllocations->end(), pos, size);
        }

        assert(false);

        return nullptr;
    }

    /**
     * Frees previously allocated data.
     *
     * @param data allocated data to free.
     */
    void Free(void* data)
    {
        LockGuard lock {mMutex};

        [[maybe_unused]] auto curSize = mAllocations->Size();
        mAllocations->RemoveIf([data](const Allocation& allocation) { return allocation.Data() == data; });
        [[maybe_unused]] auto newSize = mAllocations->Size();

        assert(curSize != newSize);
    }

    /**
     * Finds allocation by data.
     *
     * @param data allocated data.
     * @return List<Allocation>::Iterator.
     */
    RetWithError<List<Allocation>::Iterator> FindAllocation(const void* data)
    {
        LockGuard lock {mMutex};

        return mAllocations->FindIf([data](const Allocation& allocation) { return allocation.Data() == data; });
    }

    /**
     * Increases allocation shared count.
     *
     * @param it allocation to increase shared count.
     * @return size_t allocation shared count.
     */
    size_t TakeAllocation(List<Allocation>::Iterator it) { return it->Take(mMutex); }

    /**
     * Decreases allocation shared count.
     *
     * @param it allocation to increase shared count.
     * @return size_t allocation shared count.
     */
    size_t GiveAllocation(List<Allocation>::Iterator it) { return it->Give(mMutex); }

    /**
     * Returns allocator free size.
     *
     * @return size_t free size.
     */
    size_t FreeSize() const
    {
        LockGuard lock {mMutex};

        return mMaxSize - GetAllocatedSize();
    }

    /**
     * Return allocator max size.
     *
     * @return size_t max size.
     */
    size_t MaxSize() const
    {
        LockGuard lock {mMutex};

        return mMaxSize;
    }

    /**
     * Return max allocated size.
     *
     * @return size_t max allocated size.
     */
    size_t MaxAllocatedSize() const
    {
        LockGuard lock {mMutex};

        return mMaxAllocatedSize;
    }

    /**
     * Resets max allocated size.
     */
    void ResetMaxAllocatedSize()
    {
        LockGuard lock {mMutex};

        mMaxAllocatedSize = 0;
    }

protected:
    void SetBuffer(const Buffer& buffer, List<Allocation>& allocations)
    {
        mBuffer      = static_cast<uint8_t*>(buffer.Get());
        mMaxSize     = buffer.Size();
        mAllocations = &allocations;
        mAllocations->Clear();
    }

private:
    // cppcheck-suppress passedByValue
    void* Allocate(List<Allocation>::ConstIterator it, uint8_t* data, size_t size)
    {
        [[maybe_unused]] auto err = mAllocations->Emplace(it, Allocation(data, size));
        assert(err.IsNone());

        if (GetAllocatedSize() > mMaxAllocatedSize) {
            mMaxAllocatedSize = GetAllocatedSize();
        }

        return data;
    }

    size_t GetAllocatedSize() const
    {
        size_t allocatedSize = 0;

        for (const auto& allocation : *mAllocations) {
            allocatedSize += allocation.Size();
        }

        return allocatedSize;
    }

    uint8_t*          mBuffer           = {};
    List<Allocation>* mAllocations      = {};
    size_t            mMaxSize          = {};
    size_t            mMaxAllocatedSize = {};
    mutable Mutex     mMutex;
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
    StaticBuffer<cSize>                                mBuffer;
    StaticList<Allocator::Allocation, cNumAllocations> mAllocations;
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
