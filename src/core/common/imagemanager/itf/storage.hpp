/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IMAGEMANAGER_ITF_STORAGE_HPP_
#define AOS_CORE_COMMON_IMAGEMANAGER_ITF_STORAGE_HPP_

#include <core/common/ocispec/itf/imagespec.hpp>
#include <core/common/types/common.hpp>

namespace aos::common::imagemanager {

/**
 * Deployable item info.
 */
struct DeployableItemInfo {
    StaticString<cIDLen>          mItemID;
    UpdateItemType                mType;
    StaticString<cVersionLen>     mVersion;
    StaticString<oci::cDigestLen> mRootDigest;
    ItemState                     mState;
    Time                          mTimestamp;
    Error                         mError;

    /**
     * Compares deployable item info.
     *
     * @param lhs left object.
     * @param rhs right object.
     * @return bool.
     */
    friend bool operator==(const DeployableItemInfo& lhs, const DeployableItemInfo& rhs)
    {
        return lhs.mItemID == rhs.mItemID && lhs.mType == rhs.mType && lhs.mVersion == rhs.mVersion
            && lhs.mRootDigest == rhs.mRootDigest && lhs.mState == rhs.mState && lhs.mTimestamp == rhs.mTimestamp
            && lhs.mError == rhs.mError;
    }

    /**
     * Compares deployable item info.
     *
     * @param lhs left object.
     * @param rhs right object.
     * @return bool.
     */
    friend bool operator!=(const DeployableItemInfo& lhs, const DeployableItemInfo& rhs) { return !(lhs == rhs); }
};

/**
 * Item blob info.
 */
struct ItemBlobInfo {
    StaticString<oci::cDigestLen>    mDigest;
    StaticString<oci::cMediaTypeLen> mMediaType;
    StaticString<oci::cDigestLen>    mDiffID;
    bool                             mIsLayer {};

    /**
     * Compares item blob info.
     *
     * @param lhs left object.
     * @param rhs right object.
     * @return bool.
     */
    friend bool operator==(const ItemBlobInfo& lhs, const ItemBlobInfo& rhs)
    {
        return lhs.mDigest == rhs.mDigest && lhs.mMediaType == rhs.mMediaType && lhs.mDiffID == rhs.mDiffID
            && lhs.mIsLayer == rhs.mIsLayer;
    }

    /**
     * Compares item blob info.
     *
     * @param lhs left object.
     * @param rhs right object.
     * @return bool.
     */
    friend bool operator!=(const ItemBlobInfo& lhs, const ItemBlobInfo& rhs) { return !(lhs == rhs); }
};

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
     * Adds deployable item.
     *
     * @param item deployable item info.
     * @return Error: eAlreadyExist if item with the same ID and version exists.
     */
    virtual Error AddDeployableItem(const DeployableItemInfo& item) = 0;

    /**
     * Updates deployable item. Blob references of the item are kept.
     *
     * @param item deployable item info.
     * @return Error: eNotFound if item doesn't exist.
     */
    virtual Error UpdateDeployableItem(const DeployableItemInfo& item) = 0;

    /**
     * Removes deployable item and all its blob references.
     *
     * @param itemID item ID.
     * @param version item version.
     * @return Error: eNotFound if item doesn't exist.
     */
    virtual Error RemoveDeployableItem(const String& itemID, const String& version) = 0;

    /**
     * Returns deployable item.
     *
     * @param itemID item ID.
     * @param version item version.
     * @param[out] item deployable item info.
     * @return Error: eNotFound if item doesn't exist.
     */
    virtual Error GetDeployableItem(const String& itemID, const String& version, DeployableItemInfo& item) = 0;

    /**
     * Returns all deployable items.
     *
     * @param[out] items deployable items.
     * @return Error.
     */
    virtual Error GetAllDeployableItems(Array<DeployableItemInfo>& items) = 0;

    /**
     * Returns number of stored deployable items in all states.
     *
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> GetDeployableItemsCount() = 0;

    /**
     * Adds blob reference to deployable item. Does nothing if the reference already exists.
     *
     * @param itemID item ID.
     * @param version item version.
     * @param blob blob info.
     * @return Error: eNotFound if item doesn't exist.
     */
    virtual Error AddItemBlob(const String& itemID, const String& version, const ItemBlobInfo& blob) = 0;

    /**
     * Returns blobs referenced by deployable item.
     *
     * @param itemID item ID.
     * @param version item version.
     * @param[out] blobs blobs info.
     * @return Error: eNotFound if item doesn't exist.
     */
    virtual Error GetItemBlobs(const String& itemID, const String& version, Array<ItemBlobInfo>& blobs) = 0;

    /**
     * Checks if blob is referenced by any existing deployable item, regardless of the item state. Blob references
     * left without an item are ignored.
     *
     * @param digest blob digest.
     * @return RetWithError<bool>.
     */
    virtual RetWithError<bool> IsBlobUsed(const String& digest) = 0;
};

} // namespace aos::common::imagemanager

#endif
