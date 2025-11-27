/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_GIDPOOL_HPP_
#define AOS_CORE_CM_LAUNCHER_GIDPOOL_HPP_

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
 * Max number of locked IDs simultaneously.
 */
static constexpr auto cMaxNumLockedGIDs = cMaxNumUpdateItems;

/**
 * Pool that manages group identifiers for update items.
 */
class GIDPool {
public:
    using Validator = IdentifierRangePool<cGIDRangeBegin, cGIDRangeEnd, cMaxNumLockedGIDs>::Validator;

    /**
     * Initializes the underlying identifier pool.
     *
     * @param validator validator callback.
     * @return Error.
     */
    Error Init(Validator validator) { return mPool.Init(validator); }

    /**
     * Returns a GID for an update item.
     *
     * @param itemID item ID.
     * @param gid requested GID. If 0, a new GID will be generated.
     * @return RetWithError<gid_t>.
     */
    RetWithError<gid_t> GetGID(const String& itemID, gid_t gid = 0);

    /**
     * Releases a reference for the update item GID.
     *
     * @param itemID item ID.
     * @return Error.
     */
    Error Release(const String& itemID);

    /**
     * Clears allocated GIDs.
     *
     * @return Error.
     */
    Error Clear();

private:
    struct ItemEntry {
        gid_t  mGID {};
        size_t mRefCount {};
    };

    IdentifierRangePool<cGIDRangeBegin, cGIDRangeEnd, cMaxNumLockedGIDs> mPool;
    StaticMap<StaticString<cIDLen>, ItemEntry, cMaxNumUpdateItems>       mItemGIDs;
};

} // namespace aos::cm::launcher

#endif
