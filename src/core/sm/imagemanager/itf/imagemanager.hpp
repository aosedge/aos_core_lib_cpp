/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_ITF_IMAGEMANAGER_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_ITF_IMAGEMANAGER_HPP_

#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/types/common.hpp>

namespace aos::sm::imagemanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Update item info.
 */
struct UpdateItemInfo {
    StaticString<cIDLen>          mID;
    UpdateItemType                mType;
    StaticString<cVersionLen>     mVersion;
    StaticString<oci::cDigestLen> mManifestDigest;

    /**
     * Compares update item info.
     *
     * @param rhs info to compare.
     * @return bool.
     */
    bool operator==(const UpdateItemInfo& rhs) const
    {
        return mID == rhs.mID && mType == rhs.mType && mVersion == rhs.mVersion
            && mManifestDigest == rhs.mManifestDigest;
    }

    /**
     * Compares update item info.
     *
     * @param rhs info to compare.
     * @return bool.
     */
    bool operator!=(const UpdateItemInfo& rhs) const { return !operator==(rhs); };
};

/**
 * Update item status.
 */
struct UpdateItemStatus {
    StaticString<cIDLen>      mID;
    UpdateItemType            mType;
    StaticString<cVersionLen> mVersion;
    ItemState                 mState;

    /**
     * Compares update item status.
     *
     * @param rhs status to compare.
     * @return bool.
     */
    bool operator==(const UpdateItemStatus& rhs) const
    {
        return mID == rhs.mID && mType == rhs.mType && mVersion == rhs.mVersion && mState == rhs.mState;
    }

    /**
     * Compares update item status.
     *
     * @param rhs status to compare.
     * @return bool.
     */
    bool operator!=(const UpdateItemStatus& rhs) const { return !operator==(rhs); };
};

/**
 * Image manager interface.
 */
class ImageManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageManagerItf() = default;

    /**
     * Returns all installed update items statuses.
     *
     * @param statuses list of installed update item statuses.
     * @return Error.
     */
    virtual Error GetAllInstalledItems(Array<UpdateItemStatus>& statuses) const = 0;

    /**
     * Installs update item.
     *
     * @param itemInfo update item info.
     * @return Error.
     */
    virtual Error InstallUpdateItem(const UpdateItemInfo& itemInfo) = 0;

    /**
     * Removes update item.
     *
     * @param itemID update item ID.
     * @param version update item version.
     * @return Error.
     */
    virtual Error RemoveUpdateItem(const String& itemID, const String& version) = 0;
};

/** @}*/

} // namespace aos::sm::imagemanager

#endif
