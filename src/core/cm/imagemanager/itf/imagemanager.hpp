/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGEMANAGER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGEMANAGER_HPP_

#include <core/common/crypto/itf/cryptohelper.hpp>

#include "imagestatusprovider.hpp"

namespace aos::cm::imagemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Update image info.
 */
struct UpdateImageInfo {
    ImageInfo                                 mImage;
    StaticString<cFilePathLen>                mPath;
    StaticArray<uint8_t, crypto::cSHA256Size> mSHA256;
    size_t                                    mSize {};
    crypto::DecryptInfo                       mDecryptInfo;
    crypto::SignInfo                          mSignInfo;

    /**
     * Compares update image info.
     *
     * @param other info to compare with.
     * @return bool.
     */
    bool operator==(const UpdateImageInfo& other) const
    {
        return mImage == other.mImage && mPath == other.mPath && mSHA256 == other.mSHA256 && mSize == other.mSize
            && mDecryptInfo == other.mDecryptInfo && mSignInfo == other.mSignInfo;
    }

    /**
     * Compares update image info.
     *
     * @param other info to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateImageInfo& other) const { return !operator==(other); }
};

/**
 * Update item info.
 */
struct UpdateItemInfo {
    StaticString<cIDLen>                              mID;
    UpdateItemType                                    mType;
    StaticString<cVersionLen>                         mVersion;
    StaticArray<UpdateImageInfo, cMaxNumUpdateImages> mImages;

    /**
     * Compares update item info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const UpdateItemInfo& other) const
    {
        return mID == other.mID && mType == other.mType && mVersion == other.mVersion && mImages == other.mImages;
    }

    /**
     * Compares update item info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateItemInfo& other) const { return !operator==(other); }
};

/**
 * Interface that manages update items images.
 */
class ImageManagerItf : public ImageStatusProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageManagerItf() = default;

    /**
     * Installs update items.
     *
     * @param itemsInfo update items info.
     * @param certificates list of certificates.
     * @param certificateChains list of certificate chains.
     * @param[out] statuses update items statuses.
     * @return Error.
     */
    virtual Error InstallUpdateItems(const Array<UpdateItemInfo>& itemsInfo,
        const Array<crypto::CertificateInfo>&                     certificates,
        const Array<crypto::CertificateChainInfo>& certificateChains, Array<UpdateItemStatus>& statuses)
        = 0;

    /**
     * Uninstalls update items.
     *
     * @param ids update items ID's.
     * @param[out] statuses update items statuses.
     * @return Error.
     */
    virtual Error UninstallUpdateItems(const Array<StaticString<cIDLen>>& ids, Array<UpdateItemStatus>& statuses) = 0;

    /**
     * Reverts update items.
     *
     * @param ids update items ID's.
     * @return Error.
     */
    virtual Error RevertUpdateItems(const Array<StaticString<cIDLen>>& ids, Array<UpdateItemStatus>& statuses) = 0;
};

/** @}*/

} // namespace aos::cm::imagemanager

#endif
