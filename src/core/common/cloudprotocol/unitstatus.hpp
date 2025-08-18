/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_UNITSTATUS_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_UNITSTATUS_HPP_

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
 * Max number of runtimes per node.
 */
static constexpr auto cMaxNumNodeRuntimes = AOS_CONFIG_CLOUDPROTOCOL_MAX_NUM_NODE_RUNTIMES;

/**
 * Runtime type len.
 */
static constexpr auto cRuntimeTypeLen = AOS_CONFIG_CLOUDPROTOCOL_RUNTIME_TYPE_LEN;

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

using UnitConfigStatusStaticArray = StaticArray<UnitConfigStatus, cUnitConfigStatusCount>;

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
     * @brief Compares resource info.
     *
     * @param other resource info to compare with.
     * @return bool.
     */
    bool operator!=(const ResourceInfo& other) const { return !operator==(other); }
};

using ResourceInfoStaticArray = StaticArray<ResourceInfo, cMaxNumNodeResources>;

/**
 * Runtime info.
 */
struct RuntimeInfo : public PlatformInfo {
    Identifier                    mIdentifier;
    StaticString<cRuntimeTypeLen> mRuntimeType;
    size_t                        mMaxDMIPS     = 0;
    size_t                        mTotalRAM     = 0;
    size_t                        mMaxInstances = 0;

    /**
     * Compares runtime info.
     *
     * @param other runtime info to compare with.
     * @return bool.
     */
    bool operator==(const RuntimeInfo& other) const
    {
        return mIdentifier == other.mIdentifier && mMaxDMIPS == other.mMaxDMIPS && mTotalRAM == other.mTotalRAM
            && mMaxInstances == other.mMaxInstances;
    }

    /**
     * @brief Compares runtime info.
     *
     * @param other runtime info to compare with.
     * @return bool.
     */
    bool operator!=(const RuntimeInfo& other) const { return !operator==(other); }
};

using RuntimeInfoStaticArray = StaticArray<RuntimeInfo, cMaxNumNodeRuntimes>;

/**
 * Unit node information.
 */
struct NodeInfo {
    Identifier               mIdentifier;
    Identifier               mNodeGroupSubject;
    size_t                   mMaxDMIPS = 0;
    size_t                   mTotalRAM = 0;
    OSInfo                   mOSInfo;
    CPUInfoStaticArray       mCPUs;
    PartitionInfoStaticArray mPartitions;
    ResourceInfoStaticArray  mResources;
    RuntimeInfoStaticArray   mRuntimes;
    NodeAttributeStaticArray mAttrs;
    NodeState                mState;
    Error                    mError;

    /**
     * Compares node info.
     *
     * @param other node info to compare with.
     * @return bool.
     */
    bool operator==(const NodeInfo& other) const
    {
        return mIdentifier == other.mIdentifier && mNodeGroupSubject == other.mNodeGroupSubject
            && mMaxDMIPS == other.mMaxDMIPS && mTotalRAM == other.mTotalRAM && mCPUs == other.mCPUs
            && mOSInfo == other.mOSInfo && mPartitions == other.mPartitions && mResources == other.mResources
            && mRuntimes == other.mRuntimes && mAttrs == other.mAttrs && mState == other.mState
            && mError == other.mError;
    }

    /**
     * Compares node info.
     *
     * @param other node info to compare with.
     * @return bool.
     */
    bool operator!=(const NodeInfo& other) const { return !operator==(other); }
};

using NodeInfoStaticArray = StaticArray<NodeInfo, cMaxNumNodes>;

/**
 * Image status.
 */
struct ImageStatus {
    uuid::UUID mImageID;
    ImageState mState;
    Error      mError;

    /**
     * Compares image status.
     *
     * @param other image status to compare with.
     * @return bool.
     */
    bool operator==(const ImageStatus& other) const
    {
        return mImageID == other.mImageID && mState == other.mState && mError == other.mError;
    }

    /**
     * Compares image status.
     *
     * @param other image status to compare with.
     * @return bool.
     */
    bool operator!=(const ImageStatus& other) const { return !operator==(other); }
};

using ImageStatusStaticArray = StaticArray<ImageStatus, cMaxNumUpdateImages>;

/**
 * Update item status.
 */
struct UpdateItemStatus {
    Identifier                mIdentifier;
    StaticString<cVersionLen> mVersion;
    ImageStatusStaticArray    mStatuses;

    /**
     * Compares update item status.
     *
     * @param other update item status to compare with.
     * @return bool.
     */
    bool operator==(const UpdateItemStatus& other) const
    {
        return mIdentifier == other.mIdentifier && mVersion == other.mVersion && mStatuses == other.mStatuses;
    }

    /**
     * @brief Compares update item status.
     *
     * @param other update item status to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateItemStatus& other) const { return !operator==(other); }
};

using UpdateItemStatusStaticArray = StaticArray<UpdateItemStatus, cMaxNumUpdateItems>;

/**
 * Instance status.
 */
struct InstanceStatus : public PlatformInfo {
    Identifier                        mNode;
    Identifier                        mRuntime;
    uint64_t                          mInstance = 0;
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

using InstanceStatusStaticArray = StaticArray<InstanceStatus, cMaxNumInstances>;

/**
 * Instances statuses.
 */
struct InstancesStatuses {
    Identifier                mIdentifier;
    Identifier                mSubject;
    StaticString<cVersionLen> mVersion;
    InstanceStatusStaticArray mInstances;

    /**
     * Compare instances statuses.
     *
     * @param other instances statuses to compare with.
     * @return bool.
     */
    bool operator==(const InstancesStatuses& other) const
    {
        return mIdentifier == other.mIdentifier && mSubject == other.mSubject && mVersion == other.mVersion
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

using InstancesStatusesStaticArray = StaticArray<InstancesStatuses, cMaxNumUpdateItems>;

/*
 * Subjects.
 */
using SubjectStaticArray = StaticArray<Identifier, cMaxNumSubjects>;

/**
 * Update image status.
 */
struct UpdateImageStatus {
    uuid::UUID  mImageID;
    ImageStatus mStatus;
    Error       mError;

    /**
     * Compares image status.
     *
     * @param other image status to compare with.
     * @return bool.
     */
    bool operator==(const UpdateImageStatus& other) const
    {
        return mImageID == other.mImageID && mStatus == other.mStatus && mError == other.mError;
    }

    /**
     * Compares image status.
     *
     * @param other image status to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateImageStatus& other) const { return !operator==(other); }
};

/**
 * Update item status.
 */
struct UpdateItemStatus {
    Identifier                                          mIdentifier;
    StaticString<cVersionLen>                           mVersion;
    StaticArray<UpdateImageStatus, cMaxNumUpdateImages> mStatuses;

    /**
     * Compares update item status.
     *
     * @param other update item status to compare with.
     * @return bool.
     */
    bool operator==(const UpdateItemStatus& other) const
    {
        return mIdentifier == other.mIdentifier && mVersion == other.mVersion && mStatuses == other.mStatuses;
    }

    /**
     * @brief Compares update item status.
     *
     * @param other update item status to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateItemStatus& other) const { return !operator==(other); }
};

/**
 * Unit status
 */
struct UnitStatus {
    bool                                   mIsDeltaInfo {};
    Optional<UnitConfigStatusStaticArray>  mUnitConfig;
    Optional<NodeInfoStaticArray>          mNodes;
    Optional<UpdateItemStatusStaticArray>  mUpdateItems;
    Optional<InstancesStatusesStaticArray> mInstances;
    Optional<SubjectStaticArray>           mUnitSubjects;

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
