/**
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_DESIREDSTATUS_HPP_
#define AOS_CORE_COMMON_TYPES_DESIREDSTATUS_HPP_

#include <core/common/crypto/cryptohelper.hpp>
#include <core/common/ocispec/itf/imagespec.hpp>

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
    friend bool operator==(const DesiredNodeStateInfo& lhs, const DesiredNodeStateInfo& rhs)
    {
        return lhs.mNodeID == rhs.mNodeID && lhs.mState == rhs.mState;
    };

    /**
     * Compares desired nodes states.
     *
     * @param rhs desired nodes state to compare.
     * @return bool.
     */
    friend bool operator!=(const DesiredNodeStateInfo& lhs, const DesiredNodeStateInfo& rhs) { return !(lhs == rhs); };
};

using DesiredNodeStateInfoArray = StaticArray<DesiredNodeStateInfo, cMaxNumNodes>;

/**
 * Update item info.
 */
struct UpdateItemInfo {
    StaticString<cIDLen>          mItemID;
    UpdateItemType                mType;
    StaticString<cVersionLen>     mVersion;
    StaticString<cIDLen>          mOwnerID;
    StaticString<oci::cDigestLen> mIndexDigest;

    /**
     * Compares update item info.
     *
     * @return bool.
     */
    friend bool operator==(const UpdateItemInfo& lhs, const UpdateItemInfo& rhs)
    {
        return lhs.mItemID == rhs.mItemID && lhs.mOwnerID == rhs.mOwnerID && lhs.mVersion == rhs.mVersion
            && lhs.mIndexDigest == rhs.mIndexDigest && lhs.mType == rhs.mType;
    };

    /**
     * Compares update item info.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const UpdateItemInfo& lhs, const UpdateItemInfo& rhs) { return !(lhs == rhs); };
};

using UpdateItemInfoArray = StaticArray<UpdateItemInfo, cMaxNumUpdateItems>;

using LabelsArray = StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels>;

/**
 * Desired instance info.
 */
struct DesiredInstanceInfo {
    StaticString<cIDLen> mItemID;
    StaticString<cIDLen> mSubjectID;
    uint64_t             mPriority {};
    size_t               mNumInstances {};
    LabelsArray          mLabels;

    /**
     * Compares instance info.
     *
     * @param rhs desired instance info to compare.
     * @return bool.
     */
    friend bool operator==(const DesiredInstanceInfo& lhs, const DesiredInstanceInfo& rhs)
    {
        return lhs.mItemID == rhs.mItemID && lhs.mSubjectID == rhs.mSubjectID && lhs.mPriority == rhs.mPriority
            && lhs.mNumInstances == rhs.mNumInstances && lhs.mLabels == rhs.mLabels;
    };

    /**
     * Compares instance info.
     *
     * @param rhs desired instance info to compare.
     * @return bool.
     */
    friend bool operator!=(const DesiredInstanceInfo& lhs, const DesiredInstanceInfo& rhs) { return !(lhs == rhs); };
};

using DesiredInstanceInfoArray = StaticArray<DesiredInstanceInfo, cMaxNumInstances>;

/**
 * Desired status.
 */
struct DesiredStatus : public Protocol {
    DesiredNodeStateInfoArray         mNodes;
    Optional<UnitConfig>              mUnitConfig;
    UpdateItemInfoArray               mUpdateItems;
    DesiredInstanceInfoArray          mInstances;
    SubjectInfoArray                  mSubjects;
    crypto::CertificateInfoArray      mCertificates;
    crypto::CertificateChainInfoArray mCertificateChains;

    /**
     * Compares desired status.
     *
     * @param rhs desired status to compare with.
     * @return bool.
     */
    friend bool operator==(const DesiredStatus& lhs, const DesiredStatus& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mNodes == rhs.mNodes
            && lhs.mUnitConfig == rhs.mUnitConfig && lhs.mUpdateItems == rhs.mUpdateItems
            && lhs.mInstances == rhs.mInstances && lhs.mSubjects == rhs.mSubjects
            && lhs.mCertificates == rhs.mCertificates && lhs.mCertificateChains == rhs.mCertificateChains;
    };

    /**
     * Compares desired status.
     *
     * @param rhs desired status to compare with.
     * @return bool.
     */
    friend bool operator!=(const DesiredStatus& lhs, const DesiredStatus& rhs) { return !(lhs == rhs); };
};

} // namespace aos

#endif
