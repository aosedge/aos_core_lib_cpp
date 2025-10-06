/**
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_DESIREDSTATUS_HPP_
#define AOS_CORE_COMMON_TYPES_DESIREDSTATUS_HPP_

#include <core/common/crypto/cryptohelper.hpp>

#include "unitconfig.hpp"

namespace aos {

/**
 * Desired node state.
 */
class DesiredNodeStateType {
public:
    enum class Enum {
        eProvisioned,
        ePaused,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "provisioned",
            "paused",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using DesiredNodeStateEnum = DesiredNodeStateType::Enum;
using DesiredNodeState     = EnumStringer<DesiredNodeStateType>;

/**
 * Desired node state info.
 */
struct DesiredNodeStateInfo {
    StaticString<cIDLen> mNodeID;
    DesiredNodeState     mState;

    /**
     * Compares desired nodes states.
     *
     * @param rhs nodes state to compare.
     * @return bool.
     */
    bool operator==(const DesiredNodeStateInfo& rhs) const { return mNodeID == rhs.mNodeID && mState == rhs.mState; }

    /**
     * Compares desired nodes states.
     *
     * @param rhs desired nodes state to compare.
     * @return bool.
     */
    bool operator!=(const DesiredNodeStateInfo& rhs) const { return !operator==(rhs); }
};

using DesiredNodeStateInfoArray = StaticArray<DesiredNodeStateInfo, cMaxNumNodes>;

/**
 * Update image info.
 */
struct UpdateImageInfo {
    ImageInfo                                       mImage;
    StaticArray<StaticString<cURLLen>, cMaxNumURLs> mURLs;
    StaticArray<uint8_t, crypto::cSHA256Size>       mSHA256;
    size_t                                          mSize {};
    crypto::DecryptInfo                             mDecryptInfo;
    crypto::SignInfo                                mSignInfo;

    /**
     * Compares update image info.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const UpdateImageInfo& rhs) const
    {
        return mImage == rhs.mImage && mURLs == rhs.mURLs && mSHA256 == rhs.mSHA256 && mSize == rhs.mSize
            && mDecryptInfo == rhs.mDecryptInfo && mSignInfo == rhs.mSignInfo;
    }

    /**
     * Compares update item info.
     */
    bool operator!=(const UpdateImageInfo& rhs) const { return !operator==(rhs); }
};

using UpdateImageInfoArray = StaticArray<UpdateImageInfo, cMaxNumUpdateImages>;

/**
 * Update item info.
 */
struct UpdateItemInfo {
    StaticString<cIDLen>      mItemID;
    StaticString<cIDLen>      mOwnerID;
    StaticString<cVersionLen> mVersion;
    UpdateImageInfoArray      mImages;

    /**
     * Compares update item info.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const UpdateItemInfo& rhs) const
    {
        return mItemID == rhs.mItemID && mOwnerID == rhs.mOwnerID && mVersion == rhs.mVersion && mImages == rhs.mImages;
    }

    /**
     * Compares update item info.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateItemInfo& rhs) const { return !operator==(rhs); }
};

using UpdateItemInfoArray = StaticArray<UpdateItemInfo, cMaxNumUpdateItems>;

using LabelsArray = StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels>;

/**
 * Desired instance info.
 */
struct DesiredInstanceInfo {
    StaticString<cIDLen> mItemID;
    StaticString<cIDLen> mSubjectID;
    UpdateItemType       mItemType;
    uint64_t             mPriority {};
    size_t               mNumInstances {};
    LabelsArray          mLabels;

    /**
     * Compares instance info.
     *
     * @param rhs desired instance info to compare.
     * @return bool.
     */
    bool operator==(const DesiredInstanceInfo& rhs) const
    {
        return mItemID == rhs.mItemID && mSubjectID == rhs.mSubjectID && mItemType == rhs.mItemType
            && mPriority == rhs.mPriority && mNumInstances == rhs.mNumInstances && mLabels == rhs.mLabels;
    }

    /**
     * Compares instance info.
     *
     * @param rhs desired instance info to compare.
     * @return bool.
     */
    bool operator!=(const DesiredInstanceInfo& rhs) const { return !operator==(rhs); }
};

using DesiredInstanceInfoArray = StaticArray<DesiredInstanceInfo, cMaxNumInstances>;

/**
 * Desired status.
 */
struct DesiredStatus {
    DesiredNodeStateInfoArray         mNodes;
    Optional<UnitConfig>              mUnitConfig;
    UpdateItemInfoArray               mUpdateItems;
    DesiredInstanceInfoArray          mInstances;
    crypto::CertificateInfoArray      mCertificates;
    crypto::CertificateChainInfoArray mCertificateChains;

    /**
     * Compares desired status.
     *
     * @param rhs desired status to compare with.
     * @return bool.
     */
    bool operator==(const DesiredStatus& rhs) const
    {
        return mUnitConfig == rhs.mUnitConfig && mNodes == rhs.mNodes && mUpdateItems == rhs.mUpdateItems
            && mInstances == rhs.mInstances && mCertificates == rhs.mCertificates
            && mCertificateChains == rhs.mCertificateChains;
    }

    /**
     * Compares desired status.
     *
     * @param rhs desired status to compare with.
     * @return bool.
     */
    bool operator!=(const DesiredStatus& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
