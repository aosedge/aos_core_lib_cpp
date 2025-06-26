/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/cm/identifierpool/identifierpool.hpp"

namespace aos::cm::identifierpool {

/***********************************************************************************************************************
 * IdentifierPool
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error IdentifierPool::Init(IdValidator idValidator)
{
    if (!idValidator) {
        return ErrorEnum::eInvalidArgument;
    }

    mIdValidator = idValidator;

    return ErrorEnum::eNone;
}

RetWithError<size_t> IdentifierPool::GetFreeID()
{
    LockGuard lock {mMutex};

    for (size_t id = cIdsRangeBegin; id < cIdsRangeEnd; id++) {
        if (mLockedIds.Find(id) != mLockedIds.end()) {
            continue;
        }

        if (!mIdValidator || !mIdValidator(id)) {
            continue;
        }

        if (auto err = mLockedIds.PushBack(id); !err.IsNone()) {
            return {{}, AOS_ERROR_WRAP(err)};
        }

        return id;
    }

    return {{}, ErrorEnum::eNotFound};
}

Error IdentifierPool::LockID(size_t id)
{
    LockGuard lock {mMutex};

    if (id < cIdsRangeBegin || id >= cIdsRangeEnd) {
        return ErrorEnum::eOutOfRange;
    }

    if (mLockedIds.Find(id) != mLockedIds.end()) {
        return ErrorEnum::eFailed;
    }

    return mLockedIds.PushBack(id);
}

Error IdentifierPool::ReleaseID(size_t id)
{
    LockGuard lock {mMutex};

    auto it = mLockedIds.Find(id);
    if (it == mLockedIds.end()) {
        return ErrorEnum::eNotFound;
    }

    mLockedIds.Erase(it);

    return ErrorEnum::eNone;
}

} // namespace aos::cm::identifierpool
