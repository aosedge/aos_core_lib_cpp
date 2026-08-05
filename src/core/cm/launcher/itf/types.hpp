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

/** @addtogroup cm Communication Manager
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
    friend bool operator==(const InstanceInfo& lhs, const InstanceInfo& rhs)
    {
        return lhs.mInstanceIdent == rhs.mInstanceIdent && lhs.mManifestDigest == rhs.mManifestDigest
            && lhs.mNodeID == rhs.mNodeID && lhs.mPrevNodeID == rhs.mPrevNodeID && lhs.mRuntimeID == rhs.mRuntimeID
            && lhs.mUID == rhs.mUID && lhs.mGID == rhs.mGID && lhs.mTimestamp == rhs.mTimestamp
            && lhs.mState == rhs.mState && lhs.mIsUnitSubject == rhs.mIsUnitSubject && lhs.mVersion == rhs.mVersion
            && lhs.mOwnerID == rhs.mOwnerID && lhs.mSubjectType == rhs.mSubjectType && lhs.mLabels == rhs.mLabels
            && lhs.mPriority == rhs.mPriority && lhs.mDisableRebalancing == rhs.mDisableRebalancing;
    };

    /**
     * Compares instance info.
     *
     * @param rhs instance info to compare with.
     * @return bool.
     */
    friend bool operator!=(const InstanceInfo& lhs, const InstanceInfo& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const RunInstanceRequest& lhs, const RunInstanceRequest& other)
    {
        return lhs.mItemID == other.mItemID && lhs.mUpdateItemType == other.mUpdateItemType
            && lhs.mVersion == other.mVersion && lhs.mOwnerID == other.mOwnerID
            && lhs.mSubjectInfo == other.mSubjectInfo && lhs.mPriority == other.mPriority
            && lhs.mNumInstances == other.mNumInstances && lhs.mLabels == other.mLabels;
    };

    /**
     * Compares run instance request.
     *
     * @param other run instance request to compare.
     * @return bool.
     */
    friend bool operator!=(const RunInstanceRequest& lhs, const RunInstanceRequest& other) { return !(lhs == other); };
};

/** @}*/

} // namespace aos::cm::launcher

#endif
