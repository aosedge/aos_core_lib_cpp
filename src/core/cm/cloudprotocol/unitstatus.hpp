/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_CLOUDPROTOCOL_UNITSTATUS_HPP_
#define AOS_CORE_CM_CLOUDPROTOCOL_UNITSTATUS_HPP_

#include <core/common/crypto/crypto.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/types/types.hpp>

#include "common.hpp"

namespace aos::cloudprotocol {

/**
 * Annotations length.
 */
static constexpr auto cAnnotationsLen = AOS_CONFIG_CLOUDPROTOCOL_ANNOTATION_LEN;

/**
 * Component ID len.
 */
static constexpr auto cComponentIDLen = AOS_CONFIG_CLOUDPROTOCOL_COMPONENT_ID_LEN;

/**
 * Component type len.
 */
static constexpr auto cComponentTypeLen = AOS_CONFIG_CLOUDPROTOCOL_COMPONENT_TYPE_LEN;

/**
 * Unit config status count.
 */
static constexpr auto cUnitConfigStatusCount = AOS_CONFIG_CLOUDPROTOCOL_UNIT_CONFIG_STATUS_COUNT;

/**
 * Unit config status.
 */
struct UnitConfigStatus {
    StaticString<cVersionLen> mVersion;
    UnitConfigState           mState;
    Error                     mError;

    /**
     * Compares unit config status.
     *
     * @param other unit config status to compare with.
     * @return bool.
     */
    bool operator==(const UnitConfigStatus& other) const
    {
        return mVersion == other.mVersion && mState == other.mState && mError == other.mError;
    }

    bool operator!=(const UnitConfigStatus& other) const { return !operator==(other); }
};

using UnitConfigStatusArray = StaticArray<UnitConfigStatus, cUnitConfigStatusCount>;

/**
 * Resource info.
 */
struct ResourceInfo {
    StaticString<cResourceNameLen> mName;
    size_t                         mSharedCount {0};

    /**
     * Compares resource info.
     *
     * @param other resource info to compare with.
     * @return bool.
     */
    bool operator==(const ResourceInfo& other) const
    {
        return mName == other.mName && mSharedCount == other.mSharedCount;
    }

    /**
     * Compares resource info.
     *
     * @param other resource info to compare with.
     * @return bool.
     */
    bool operator!=(const ResourceInfo& other) const { return !operator==(other); }
};

using ResourceInfoArray = StaticArray<ResourceInfo, cMaxNumNodeResources>;

/**
 * Runtime info.
 */
struct RuntimeInfo : public PlatformInfo {
    Identity                      mIdentity;
    StaticString<cRuntimeTypeLen> mRuntimeType;
    Optional<size_t>              mMaxDMIPS;
    Optional<size_t>              mAllowedDMIPS;
    Optional<size_t>              mTotalRAM;
    Optional<ssize_t>             mAllowedRAM;
    size_t                        mMaxInstances = 0;

    /**
     * Compares runtime info.
     *
     * @param other runtime info to compare with.
     * @return bool.
     */
    bool operator==(const RuntimeInfo& other) const
    {
        return mIdentity == other.mIdentity && mMaxDMIPS == other.mMaxDMIPS && mTotalRAM == other.mTotalRAM
            && mMaxInstances == other.mMaxInstances;
    }

    /**
     * Compares runtime info.
     *
     * @param other runtime info to compare with.
     * @return bool.
     */
    bool operator!=(const RuntimeInfo& other) const { return !operator==(other); }
};

using RuntimeInfoArray = StaticArray<RuntimeInfo, cMaxNumNodeRuntimes>;

/**
 * Unit node information.
 */
struct NodeInfo {
    Identity           mIdentity;
    Identity           mNodeGroupSubject;
    size_t             mMaxDMIPS {};
    size_t             mTotalRAM {};
    Optional<size_t>   mPhysicalRAM;
    OSInfo             mOSInfo;
    CPUInfoArray       mCPUs;
    PartitionInfoArray mPartitions;
    ResourceInfoArray  mResources;
    RuntimeInfoArray   mRuntimes;
    NodeAttributeArray mAttrs;
    bool               mProvisioned = false;
    NodeState          mState;
    Error              mError;

    /**
     * Compares node info.
     *
     * @param other node info to compare with.
     * @return bool.
     */
    bool operator==(const NodeInfo& other) const
    {
        return mIdentity == other.mIdentity && mNodeGroupSubject == other.mNodeGroupSubject
            && mMaxDMIPS == other.mMaxDMIPS && mTotalRAM == other.mTotalRAM && mCPUs == other.mCPUs
            && mOSInfo == other.mOSInfo && mPartitions == other.mPartitions && mResources == other.mResources
            && mRuntimes == other.mRuntimes && mAttrs == other.mAttrs && mProvisioned == other.mProvisioned
            && mState == other.mState && mError == other.mError;
    }

    /**
     * Compares node info.
     *
     * @param other node info to compare with.
     * @return bool.
     */
    bool operator!=(const NodeInfo& other) const { return !operator==(other); }
};

using NodeInfoArray = StaticArray<NodeInfo, cMaxNumNodes>;

/**
 * Update item status.
 */
struct UpdateItemStatus {
    Identity                  mIdentity;
    StaticString<cVersionLen> mVersion;
    ImageStatusArray          mStatuses;

    /**
     * Compares update item status.
     *
     * @param other update item status to compare with.
     * @return bool.
     */
    bool operator==(const UpdateItemStatus& other) const
    {
        return mIdentity == other.mIdentity && mVersion == other.mVersion && mStatuses == other.mStatuses;
    }

    /**
     * Compares update item status.
     *
     * @param other update item status to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateItemStatus& other) const { return !operator==(other); }
};

using UpdateItemStatusArray = StaticArray<UpdateItemStatus, cMaxNumUpdateItems>;

/**
 * Instance status.
 */
struct InstanceStatus : public PlatformInfo {
    Identity                          mNode;
    Identity                          mRuntime;
    uint64_t                          mInstance {};
    StaticArray<uint8_t, cSHA256Size> mStateChecksum;
    InstanceState                     mState;
    Error                             mError;

    /**
     * Compares instance status.
     *
     * @param other instance status to compare with.
     * @return bool.
     */
    bool operator==(const InstanceStatus& other) const
    {
        return mNode == other.mNode && mRuntime == other.mRuntime && mInstance == other.mInstance
            && mStateChecksum == other.mStateChecksum && mState == other.mState && mError == other.mError;
    }

    /**
     * Compares instance status.
     *
     * @param other instance status to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceStatus& other) const { return !operator==(other); }
};

using InstanceStatusArray = StaticArray<InstanceStatus, cMaxNumInstances>;

/**
 * Instances statuses.
 */
struct InstancesStatuses {
    Identity                  mIdentity;
    Identity                  mSubject;
    StaticString<cVersionLen> mVersion;
    InstanceStatusArray       mInstances;

    /**
     * Compare instances statuses.
     *
     * @param other instances statuses to compare with.
     * @return bool.
     */
    bool operator==(const InstancesStatuses& other) const
    {
        return mIdentity == other.mIdentity && mSubject == other.mSubject && mVersion == other.mVersion
            && mInstances == other.mInstances;
    }

    /**
     * Compares instances statuses.
     *
     * @param other instances statuses to compare with.
     * @return bool.
     */
    bool operator!=(const InstancesStatuses& other) const { return !operator==(other); }
};

using InstancesStatusesArray = StaticArray<InstancesStatuses, cMaxNumUpdateItems>;

/*
 * Subjects.
 */
using SubjectArray = StaticArray<Identity, cMaxNumSubjects>;

/**
 * Unit status
 */
struct UnitStatus {
    bool                             mIsDeltaInfo {};
    Optional<UnitConfigStatusArray>  mUnitConfig;
    Optional<NodeInfoArray>          mNodes;
    Optional<UpdateItemStatusArray>  mUpdateItems;
    Optional<InstancesStatusesArray> mInstances;
    Optional<SubjectArray>           mUnitSubjects;

    /**
     * Compares unit status.
     *
     * @param status unit status to compare with.
     * @return bool.
     */
    bool operator==(const UnitStatus& status) const
    {
        return mUnitConfig == status.mUnitConfig && mNodes == status.mNodes && mUnitSubjects == status.mUnitSubjects
            && mUpdateItems == status.mUpdateItems;
    }

    /**
     * Compares unit status.
     *
     * @param status unit status to compare with.
     * @return bool.
     */
    bool operator!=(const UnitStatus& status) const { return !operator==(status); }
};

} // namespace aos::cloudprotocol

#endif
