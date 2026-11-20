/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_TESTS_STUBS_FAILINGALLOCATOR_HPP_
#define AOS_CORE_SM_LAUNCHER_TESTS_STUBS_FAILINGALLOCATOR_HPP_

#include <cstdlib>
#include <mutex>

#include <core/common/tools/memory.hpp>

namespace aos::sm::launcher {

class FailingAllocator : public AllocatorItf {
public:
    static constexpr size_t cAnySize = 0;

    void Fail(size_t size, size_t skip = 0, size_t count = 1)
    {
        std::lock_guard lock {mMutex};

        mFailSize  = size;
        mSkip      = skip;
        mFailCount = count;
        mNumFailed = 0;
    }

    size_t NumFailed()
    {
        std::lock_guard lock {mMutex};

        return mNumFailed;
    }

    void* Allocate(size_t size) override
    {
        std::lock_guard lock {mMutex};

        if (mFailCount != 0 && (mFailSize == cAnySize || mFailSize == size)) {
            if (mSkip != 0) {
                mSkip--;
            } else {
                mFailCount--;
                mNumFailed++;

                return nullptr;
            }
        }

        return std::malloc(size);
    }

    void Free(void* data) override { std::free(data); }

private:
    std::mutex mMutex;
    size_t     mFailSize {};
    size_t     mSkip {};
    size_t     mFailCount {};
    size_t     mNumFailed {};
};

} // namespace aos::sm::launcher

#endif
