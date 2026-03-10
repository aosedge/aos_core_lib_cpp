/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_ITF_TYPES_HPP_
#define AOS_CM_LAUNCHER_ITF_TYPES_HPP_

#include <core/common/ocispec/itf/imagespec.hpp>
#include <core/common/types/common.hpp>
#include <core/common/types/desiredstatus.hpp>

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Supported hash functions.
 */
class InstanceStateType {
public:
    enum class Enum { eActive, eDisabled, eCached };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sInstanceStateStrings[] = {
            "active",
            "disabled",
            "cached",
        };
        return Array<const char* const>(sInstanceStateStrings, ArraySize(sInstanceStateStrings));
    };
};

using InstanceStateEnum = InstanceStateType::Enum;
using InstanceState     = EnumStringer<InstanceStateType>;

/**
 * Persisted instance information.
 */
struct InstanceInfo {
    InstanceIdent                 mInstanceIdent;
    StaticString<oci::cDigestLen> mManifestDigest;
    StaticString<cIDLen>          mNodeID;
    StaticString<cIDLen>          mPrevNodeID;
    StaticString<cIDLen>          mRuntimeID;
    uid_t                         mUID {};
    gid_t                         mGID {};
    Time                          mTimestamp;
    InstanceState                 mState {};
    bool                          mIsUnitSubject {};
    StaticString<cVersionLen>     mVersion;
    StaticString<cIDLen>          mOwnerID;
    SubjectType                   mSubjectType;
    LabelsArray                   mLabels;
    size_t                        mPriority {};
    bool                          mDisableRebalancing {};

    /**
     * Compares instance info.
     *
     * @param other instance info to compare with.
     * @return bool.
     */
    bool operator==(const InstanceInfo& rhs) const
    {
        return mInstanceIdent == rhs.mInstanceIdent && mManifestDigest == rhs.mManifestDigest && mNodeID == rhs.mNodeID
            && mPrevNodeID == rhs.mPrevNodeID && mRuntimeID == rhs.mRuntimeID && mUID == rhs.mUID && mGID == rhs.mGID
            && mTimestamp == rhs.mTimestamp && mState == rhs.mState && mIsUnitSubject == rhs.mIsUnitSubject
            && mVersion == rhs.mVersion && mOwnerID == rhs.mOwnerID && mSubjectType == rhs.mSubjectType
            && mLabels == rhs.mLabels && mPriority == rhs.mPriority && mDisableRebalancing == rhs.mDisableRebalancing;
    }

    /**
     * Compares instance info.
     *
     * @param rhs instance info to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceInfo& rhs) const { return !operator==(rhs); }
};

/*
 * Run instance request.
 */
struct RunInstanceRequest {
    StaticString<cIDLen>      mItemID;
    UpdateItemType            mUpdateItemType;
    StaticString<cVersionLen> mVersion;
    StaticString<cIDLen>      mOwnerID;
    SubjectInfo               mSubjectInfo;
    size_t                    mPriority {};
    size_t                    mNumInstances {};
    LabelsArray               mLabels;

    /**
     * Compares run instance request.
     *
     * @param other run instance request to compare.
     * @return bool.
     */
    bool operator==(const RunInstanceRequest& other) const
    {
        return mItemID == other.mItemID && mUpdateItemType == other.mUpdateItemType && mVersion == other.mVersion
            && mOwnerID == other.mOwnerID && mSubjectInfo == other.mSubjectInfo && mPriority == other.mPriority
            && mNumInstances == other.mNumInstances && mLabels == other.mLabels;
    }

    /**
     * Compares run instance request.
     *
     * @param other run instance request to compare.
     * @return bool.
     */
    bool operator!=(const RunInstanceRequest& other) const { return !operator==(other); }
};

/** @}*/

} // namespace aos::cm::launcher

#endif
