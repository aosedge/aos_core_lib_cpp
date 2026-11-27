/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_STUBS_FAILINGALLOCATOR_HPP_
#define AOS_CORE_COMMON_TESTS_STUBS_FAILINGALLOCATOR_HPP_

#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>

#include <core/common/tools/heapallocator.hpp>

namespace aos::tests {

/**
 * Thread-safe heap allocator that can be configured to fail allocations.
 *
 * By default all allocations succeed. Arming the allocator with Fail(), FailAt() or FailAll() makes the matching
 * allocations return nullptr and resets the failed allocations counter. Reset() disarms the allocator but keeps the
 * counter, so it can be checked afterwards.
 */
class FailingAllocator : public AllocatorItf {
public:
    /**
     * Size value that matches allocation of any size.
     */
    static constexpr size_t cAnySize = 0;

    /**
     * Count value that makes all matching allocations fail.
     */
    static constexpr size_t cUnlimited = std::numeric_limits<size_t>::max();

    /**
     * Makes allocations of the given size fail.
     *
     * @param size size of allocations to fail, cAnySize matches any size.
     * @param skip number of matching allocations to succeed before the first failure.
     * @param count number of matching allocations to fail, cUnlimited fails all of them.
     */
    void Fail(size_t size, size_t skip = 0, size_t count = 1)
    {
        std::lock_guard lock {mMutex};

        mFailSize  = size;
        mSkip      = skip;
        mFailCount = count;
        mNumFailed = 0;
    }

    /**
     * Makes the allocation with the given index (0-based, counted from this call) fail.
     *
     * @param index index of allocation to fail.
     * @param count number of consecutive allocations to fail, cUnlimited fails all subsequent allocations.
     */
    void FailAt(size_t index, size_t count = 1) { Fail(cAnySize, index, count); }

    /**
     * Makes all allocations fail until Reset() is called.
     */
    void FailAll() { Fail(cAnySize, 0, cUnlimited); }

    /**
     * Disarms the allocator so allocations succeed again. The failed allocations counter is kept.
     */
    void Reset()
    {
        std::lock_guard lock {mMutex};

        mFailCount = 0;
    }

    /**
     * Returns number of failed allocations since the allocator was armed.
     *
     * @return size_t.
     */
    size_t NumFailed() const
    {
        std::lock_guard lock {mMutex};

        return mNumFailed;
    }

    /**
     * Waits until at least the given number of allocations failed since the allocator was armed.
     *
     * @param count number of failed allocations to wait for.
     * @param timeout wait timeout.
     * @return bool true if the allocations failed before timeout.
     */
    template <typename Rep, typename Period>
    bool WaitNumFailed(size_t count, const std::chrono::duration<Rep, Period>& timeout)
    {
        std::unique_lock lock {mMutex};

        return mCondVar.wait_for(lock, timeout, [&]() { return mNumFailed >= count; });
    }

    /**
     * Allocates memory.
     *
     * @param size size to allocate.
     * @return void*.
     */
    void* Allocate(size_t size) override
    {
        {
            std::lock_guard lock {mMutex};

            if (mFailCount != 0 && (mFailSize == cAnySize || mFailSize == size)) {
                if (mSkip != 0) {
                    mSkip--;
                } else {
                    if (mFailCount != cUnlimited) {
                        mFailCount--;
                    }

                    mNumFailed++;
                    mCondVar.notify_all();

                    return nullptr;
                }
            }
        }

        return mHeap.Allocate(size);
    }

    /**
     * Frees memory.
     *
     * @param data pointer to free.
     */
    void Free(void* data) override { mHeap.Free(data); }

private:
    mutable std::mutex      mMutex;
    std::condition_variable mCondVar;
    HeapAllocator           mHeap;
    size_t                  mFailSize {};
    size_t                  mSkip {};
    size_t                  mFailCount {};
    size_t                  mNumFailed {};
};

} // namespace aos::tests

#endif
