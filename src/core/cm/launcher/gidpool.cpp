/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gidpool.hpp"

namespace aos::cm::launcher {

RetWithError<gid_t> GIDPool::GetGID(const String& itemID, gid_t gid)
{
    if (auto existing = mItemGIDs.Find(itemID); existing != mItemGIDs.end()) {
        if (gid != 0 && existing->mSecond.mGID != gid) {
            return {0, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
        }

        existing->mSecond.mRefCount++;

        return {existing->mSecond.mGID, ErrorEnum::eNone};
    }

    gid_t assigned = gid;

    if (gid != 0) {
        if (auto err = mPool.TryAcquire(gid); !err.IsNone()) {
            return {0, AOS_ERROR_WRAP(err)};
        }
    } else {
        auto [autoGID, acquireErr] = mPool.Acquire();
        if (!acquireErr.IsNone()) {
            return {0, AOS_ERROR_WRAP(acquireErr)};
        }

        assigned = static_cast<gid_t>(autoGID);
    }

    ItemEntry entry {assigned, 1};

    if (auto err = mItemGIDs.Emplace(itemID, entry); !err.IsNone()) {
        mPool.Release(assigned);

        return {0, AOS_ERROR_WRAP(err)};
    }

    return {assigned, ErrorEnum::eNone};
}

Error GIDPool::Release(const String& itemID)
{
    auto existing = mItemGIDs.Find(itemID);
    if (existing == mItemGIDs.end()) {
        return ErrorEnum::eNotFound;
    }

    auto& entry = existing->mSecond;

    if (entry.mRefCount > 1) {
        entry.mRefCount--;

        return ErrorEnum::eNone;
    }

    if (auto err = mPool.Release(entry.mGID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return mItemGIDs.Remove(itemID);
}

Error GIDPool::Clear()
{
    if (auto err = mPool.Clear(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mItemGIDs.Clear();

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
