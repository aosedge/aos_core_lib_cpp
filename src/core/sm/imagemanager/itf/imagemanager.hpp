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
    friend bool operator==(const UpdateItemInfo& lhs, const UpdateItemInfo& rhs)
    {
        return lhs.mID == rhs.mID && lhs.mType == rhs.mType && lhs.mVersion == rhs.mVersion
            && lhs.mManifestDigest == rhs.mManifestDigest;
    };

    /**
     * Compares update item info.
     *
     * @param rhs info to compare.
     * @return bool.
     */
    friend bool operator!=(const UpdateItemInfo& lhs, const UpdateItemInfo& rhs) { return !(lhs == rhs); };
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
     * Installs update item.
     *
     * @param itemInfo update item info.
     * @return Error.
     */
    virtual Error InstallItem(const UpdateItemInfo& itemInfo) = 0;

    /**
     * Removes update item.
     *
     * @param itemID update item ID.
     * @param version update item version.
     * @return Error.
     */
    virtual Error RemoveItem(const String& itemID, const String& version) = 0;

    /**
     * Cancels all items being installed. Waits until all InstallItem and RemoveItem calls in progress return.
     *
     * @return Error.
     */
    virtual Error Cancel() = 0;
};

/** @}*/

} // namespace aos::sm::imagemanager

#endif
