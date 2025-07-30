/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TOOLS_IDENTIFIERPOOL_HPP_
#define AOS_CORE_COMMON_TOOLS_IDENTIFIERPOOL_HPP_

#include <core/common/types/types.hpp>

#include "noncopyable.hpp"
#include "thread.hpp"

namespace aos {

/**
 * Identifier range pool.
 * @tparam cStartRange pool's range start.
 * @tparam cRangeEnd pool's range end.
 * @tparam cMaxNumLockedIDs maximum number of locked identifiers.
 */
template <size_t cStartRange, size_t cRangeEnd, size_t cMaxNumLockedIDs>
class IdentifierRangePool : public NonCopyable {
public:
    /**
     * Validator function type.
     */
    using Validator = bool (*)(size_t id);

    /**
     * Initializes identifier pool.
     *
     * @param validator identifier validator functor.
     * @return Error.
     */
    Error Init(Validator validator)
    {
        mValidator = validator;

        return ErrorEnum::eNone;
    }

    /**
     * Acquires free identifier from pool.
     *
     * @return RetWithError<size_t>.
     */
    RetWithError<size_t> Acquire()
    {
        LockGuard lock {mMutex};

        if (!mValidator) {
            return {{}, AOS_ERROR_WRAP(ErrorEnum::eFailed)};
        }

        for (size_t id = cStartRange; id < cRangeEnd; id++) {
            if (mLockedIds.Find(id) != mLockedIds.end()) {
                continue;
            }

            if (!mValidator(id)) {
                continue;
            }

            if (auto err = mLockedIds.PushBack(id); !err.IsNone()) {
                return {{}, AOS_ERROR_WRAP(err)};
            }

            return id;
        }

        return {{}, ErrorEnum::eNotFound};
    }

    /**
     * Tries to acquire identifier from pool.
     *
     * @param id desired identifier to acquire.
     * @return Error.
     */
    Error TryAcquire(size_t id)
    {
        LockGuard lock {mMutex};

        if (id < cStartRange || id >= cRangeEnd) {
            return ErrorEnum::eOutOfRange;
        }

        if (mLockedIds.Find(id) != mLockedIds.end()) {
            return ErrorEnum::eFailed;
        }

        return mLockedIds.PushBack(id);
    }

    /**
     * Releases identifier in pool.
     *
     * @param id identifier to release.
     * @return Error.
     */
    Error Release(size_t id)
    {
        LockGuard lock {mMutex};

        auto it = mLockedIds.Find(id);
        if (it == mLockedIds.end()) {
            return ErrorEnum::eNotFound;
        }

        mLockedIds.Erase(it);

        return ErrorEnum::eNone;
    }

private:
    Mutex                                 mMutex;
    StaticArray<size_t, cMaxNumLockedIDs> mLockedIds;
    Validator                             mValidator {};

    static_assert(cStartRange < cRangeEnd, "invalid identifier pool range");
};

} // namespace aos

#endif
