/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_STUBS_TESTALLOCATOR_HPP_
#define AOS_CORE_COMMON_TESTS_STUBS_TESTALLOCATOR_HPP_

#include <limits>

#include <core/common/tools/heapallocator.hpp>

namespace aos::tests {

/**
 * Heap allocator that can be configured to fail after a given number of allocations.
 */
class TestAllocator : public AllocatorItf {
public:
    /**
     * Resets the allocator so allocations succeed again.
     */
    void Reset()
    {
        mCount     = 0;
        mFailAfter = std::numeric_limits<size_t>::max();
    }

    /**
     * Makes Allocate return nullptr starting from the N-th call (0-based).
     *
     * @param allocations number of successful allocations before failure.
     */
    void FailAfter(size_t allocations)
    {
        mCount     = 0;
        mFailAfter = allocations;
    }

    /**
     * Allocates memory.
     *
     * @param size size to allocate.
     * @return void*.
     */
    void* Allocate(size_t size) override
    {
        if (mCount++ >= mFailAfter) {
            return nullptr;
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
    HeapAllocator mHeap;
    size_t        mCount     = 0;
    size_t        mFailAfter = std::numeric_limits<size_t>::max();
};

} // namespace aos::tests

#endif
