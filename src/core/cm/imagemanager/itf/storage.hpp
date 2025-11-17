/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_STORAGE_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_STORAGE_HPP_

#include <core/common/types/desiredstatus.hpp>

namespace aos::cm::imagemanager {

/**
 * Update item info.
 */
struct ItemInfo {
<<<<<<< HEAD
    StaticString<cIDLen>                        mID;
    UpdateItemType                              mType;
    StaticString<cVersionLen>                   mVersion;
    ItemState                                   mState;
    StaticString<cFilePathLen>                  mPath;
    size_t                                      mTotalSize {};
    size_t                                      mGID {};
    Time                                        mTimestamp;
    StaticArray<ImageInfo, cMaxNumUpdateImages> mImages;
=======
    StaticString<cIDLen>      mItemID;
    StaticString<cVersionLen> mVersion;
    ItemState                 mState;
>>>>>>> d1d0ef2e (cm: imagemanager: add new image manager design document)

    /**
     * Compares update item info.
     *
     * @param rhs update item info to compare with.
     * @return bool.
     */
    bool operator==(const ItemInfo& rhs) const
    {
        return mItemID == rhs.mItemID && mVersion == rhs.mVersion && mState == rhs.mState;
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
     * Sets item state.
     *
     * @param id ID.
     * @param Version Version.
     * @param state Item state.
     * @return Error.
     */
    virtual Error SetItemState(const String& id, const String& Version, ItemState state) = 0;

    /**
     * Gets items info.
     *
     * @param items Items info.
     * @return Error.
     */
    virtual Error GetItemsInfo(Array<ItemInfo>& items) = 0;
};

} // namespace aos::cm::imagemanager

#endif
