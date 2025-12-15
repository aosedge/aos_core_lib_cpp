/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_STORAGE_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_STORAGE_HPP_

#include <core/common/types/common.hpp>
#include <core/common/types/desiredstatus.hpp>

namespace aos::cm::imagemanager {

/**
 * Update item info.
 */
struct ItemInfo {
    StaticString<cIDLen>          mItemID;
    StaticString<cVersionLen>     mVersion;
    StaticString<oci::cDigestLen> mIndexDigest;
    ItemState                     mState {};
    Time                          mTimestamp {};

    /**
     * Compares update item info.
     *
     * @param rhs update item info to compare with.
     * @return bool.
     */
    bool operator==(const ItemInfo& rhs) const
    {
        return mItemID == rhs.mItemID && mVersion == rhs.mVersion && mIndexDigest == rhs.mIndexDigest
            && mState == rhs.mState && mTimestamp == rhs.mTimestamp;
    }

    /**
     * Compares update item info.
     *
     * @param rhs update item info to compare with.
     * @return bool.
     */
    bool operator!=(const ItemInfo& rhs) const { return !operator==(rhs); }
};

/**
 * Storage interface.
 */
class StorageItf {
public:
    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;

    /**
     * Adds item.
     *
     * @param item Item info.
     * @return Error.
     */
    virtual Error AddItem(const ItemInfo& item) = 0;

    /**
     * Removes item.
     *
     * @param id ID.
     * @param version Version.
     * @return Error.
     */
    virtual Error RemoveItem(const String& id, const String& version) = 0;

    /**
     * Updates item state.
     *
     * @param id ID.
     * @param version Version.
     * @param state Item state.
     * @param timestamp Timestamp (optional).
     * @return Error.
     */
    virtual Error UpdateItemState(const String& id, const String& version, ItemState state, Time timestamp = {}) = 0;

    /**
     * Gets items info.
     *
     * @param items Items info.
     * @return Error.
     */
    virtual Error GetItemsInfo(Array<ItemInfo>& items) = 0;

    /**
     * Gets item info by ID.
     *
     * @param itemID Item ID.
     * @param items Items info.
     * @return Error.
     */
    virtual Error GetItemInfos(const String& itemID, Array<ItemInfo>& items) = 0;
};

} // namespace aos::cm::imagemanager

#endif
