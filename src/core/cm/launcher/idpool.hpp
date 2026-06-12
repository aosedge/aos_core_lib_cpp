/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_IDPOOL_HPP_
#define AOS_CORE_CM_LAUNCHER_IDPOOL_HPP_

#include <sys/types.h>

#include <core/common/tools/error.hpp>
#include <core/common/tools/identifierpool.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/types/common.hpp>

namespace aos::cm::launcher {

/**
 * GID range start.
 */
static constexpr auto cGIDRangeBegin = 5000;

/**
 * GID range end.
 */
static constexpr auto cGIDRangeEnd = 10000;

/**
 * Max number of locked GIDs simultaneously.
 */
static constexpr auto cMaxNumLockedGIDs = cMaxNumUpdateItems;

/**
 * UID range start.
 */
static constexpr auto cUIDRangeBegin = 5000;

/**
 * UID range end.
 */
static constexpr auto cUIDRangeEnd = 10000;

/**
 * Max number of locked UIDs simultaneously.
 */
static constexpr auto cMaxNumLockedUIDs = cMaxNumInstances;

/**
 * Pool that manages identifiers with reference counting per key.
 *
 * @tparam K key type.
 * @tparam I identifier type.
 * @tparam cRangeBegin identifier range start.
 * @tparam cRangeEnd identifier range end.
 * @tparam cMaxNumLocked max number of locked IDs simultaneously.
 * @tparam cMaxNumItems max number of tracked keys.
 */
template <typename K, typename I, size_t cRangeBegin, size_t cRangeEnd, size_t cMaxNumLocked, size_t cMaxNumItems>
class IDPool {
public:
    /**
     * Initializes the underlying identifier pool.
     *
     * @param validator validator callback.
     * @return Error.
     */
    Error Init(IdentifierPoolValidator validator) { return mPool.Init(validator); }

    /**
     * Returns an identifier for a key.
     *
     * @param key key.
     * @param defaultID requested identifier. If 0, a new identifier will be generated.
     * @return RetWithError<I>.
     */
    RetWithError<I> Acquire(const K& key, I defaultID = 0)
    {
        if (auto existing = mItems.Find(key); existing != mItems.end()) {
            if (defaultID != 0 && existing->mSecond.mID != defaultID) {
                return {0, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
            }

            existing->mSecond.mRefCount++;

            return {existing->mSecond.mID, ErrorEnum::eNone};
        }

        I assigned = defaultID;

        if (defaultID != 0) {
            if (auto err = mPool.TryAcquire(defaultID); !err.IsNone()) {
                return {0, AOS_ERROR_WRAP(err)};
            }
        } else {
            auto [autoID, acquireErr] = mPool.Acquire();
            if (!acquireErr.IsNone()) {
                return {0, AOS_ERROR_WRAP(acquireErr)};
            }

            assigned = static_cast<I>(autoID);
        }

        ItemEntry entry {assigned, 1};

        if (auto err = mItems.Emplace(key, entry); !err.IsNone()) {
            mPool.Release(assigned);

            return {0, AOS_ERROR_WRAP(err)};
        }

        return {assigned, ErrorEnum::eNone};
    }

    /**
     * Releases a reference for the key identifier.
     *
     * @param key key.
     * @return Error.
     */
    Error Release(const K& key)
    {
        auto existing = mItems.Find(key);
        if (existing == mItems.end()) {
            return ErrorEnum::eNotFound;
        }

        auto& entry = existing->mSecond;

        if (entry.mRefCount > 1) {
            entry.mRefCount--;

            return ErrorEnum::eNone;
        }

        if (auto err = mPool.Release(entry.mID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return mItems.Remove(key);
    }

    /**
     * Clears allocated identifiers.
     *
     * @return Error.
     */
    Error Clear()
    {
        if (auto err = mPool.Clear(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        mItems.Clear();

        return ErrorEnum::eNone;
    }

private:
    struct ItemEntry {
        I      mID {};
        size_t mRefCount {};
    };

    IdentifierRangePool<cRangeBegin, cRangeEnd, cMaxNumLocked> mPool;
    StaticMap<K, ItemEntry, cMaxNumItems>                      mItems;
};

using GIDPool
    = IDPool<StaticString<cIDLen>, gid_t, cGIDRangeBegin, cGIDRangeEnd, cMaxNumLockedGIDs, cMaxNumUpdateItems>;

using UIDPool = IDPool<InstanceIdent, uid_t, cUIDRangeBegin, cUIDRangeEnd, cMaxNumLockedUIDs, cMaxNumInstances>;

} // namespace aos::cm::launcher

#endif
