/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_ITF_STORAGE_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_ITF_STORAGE_HPP_

#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/types/common.hpp>

namespace aos::sm::imagemanager {

/**
 * Update item data structure.
 */
struct UpdateItemData {
    StaticString<cIDLen>          mID;
    UpdateItemType                mType;
    StaticString<cVersionLen>     mVersion;
    StaticString<oci::cDigestLen> mManifestDigest;
    ItemState                     mState;
    Time                          mTimestamp;

    /**
     * Compares update item data.
     */
    bool operator==(const UpdateItemData& rhs) const
    {
        return mID == rhs.mID && mType == rhs.mType && mVersion == rhs.mVersion
            && mManifestDigest == rhs.mManifestDigest && mState == rhs.mState && mTimestamp == rhs.mTimestamp;
    }

    /**
     * Compares update item data.
     */
    bool operator!=(const UpdateItemData& rhs) const { return !operator==(rhs); }
};

/**
 * Update item data static array type.
 *
 * @note The maximum number of update items is doubled to account for multiple versions of the same item.
 */
using UpdateItemDataStaticArray = StaticArray<UpdateItemData, cMaxNumUpdateItems * 2>;

/**
 * Image manager storage interface.
 */
class StorageItf {
public:
    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;

    /**
     * Adds update item to storage.
     *
     * @param updateItem update item to add.
     * @return Error.
     */
    virtual Error AddUpdateItem(const UpdateItemData& updateItem) = 0;

    /**
     * Updates update item in storage.
     *
     * @param updateItem update item to update.
     * @return Error.
     */
    virtual Error UpdateUpdateItem(const UpdateItemData& updateItem) = 0;

    /**
     * Removes previously stored update item.
     *
     * @param itemID update item ID to remove.
     * @param version update item version.
     * @return Error.
     */
    virtual Error RemoveUpdateItem(const String& itemID, const String& version) = 0;

    /**
     * Returns update item versions by item ID.
     *
     * @param itemID update item ID.
     * @param[out] itemData array to store update item data.
     * @return Error.
     */
    virtual Error GetUpdateItem(const String& itemID, Array<UpdateItemData>& itemData) = 0;

    /**
     * Returns all update items.
     *
     * @param itemsData[out] array to store update items data.
     * @return Error.
     */
    virtual Error GetAllUpdateItems(Array<UpdateItemData>& itemsData) = 0;

    /**
     * Returns count of stored update items.
     *
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> GetUpdateItemsCount() = 0;
};

/** @}*/

} // namespace aos::sm::imagemanager

#endif
