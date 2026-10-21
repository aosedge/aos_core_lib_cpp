/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IMAGEMANAGER_ITF_STORAGE_HPP_
#define AOS_CORE_COMMON_IMAGEMANAGER_ITF_STORAGE_HPP_

#include <core/common/ocispec/itf/imagespec.hpp>
#include <core/common/types/common.hpp>
#include <core/common/types/unitstatus.hpp>

namespace aos::common::imagemanager {

/**
 * Deployable item info.
 */
struct DeployableItemInfo : public UpdateItemStatus {
    StaticString<oci::cDigestLen> mRootDigest;
    Time                          mTimestamp;

    /**
     * Compares deployable item info.
     *
     * @param lhs left object.
     * @param rhs right object.
     * @return bool.
     */
    friend bool operator==(const DeployableItemInfo& lhs, const DeployableItemInfo& rhs)
    {
        return (static_cast<const UpdateItemStatus&>(lhs) == static_cast<const UpdateItemStatus&>(rhs))
            && lhs.mRootDigest == rhs.mRootDigest && lhs.mTimestamp == rhs.mTimestamp;
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
struct ItemBlobInfo : public oci::ContentDescriptor {
    bool                          mIsLayer {};
    StaticString<oci::cDigestLen> mDiffID;

    /**
     * Compares item blob info.
     *
     * @param lhs left object.
     * @param rhs right object.
     * @return bool.
     */
    friend bool operator==(const ItemBlobInfo& lhs, const ItemBlobInfo& rhs)
    {
        return (static_cast<const oci::ContentDescriptor&>(lhs) == static_cast<const oci::ContentDescriptor&>(rhs))
            && lhs.mIsLayer == rhs.mIsLayer && lhs.mDiffID == rhs.mDiffID;
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
