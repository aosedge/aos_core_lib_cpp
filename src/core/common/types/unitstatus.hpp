/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_UNITSTATUS_HPP_
#define AOS_CORE_COMMON_TYPES_UNITSTATUS_HPP_

#include "instance.hpp"
#include "unitconfig.hpp"

namespace aos {

/**
 * Unit config status count.
 */
constexpr auto cUnitConfigStatusCount = 2;

using UnitConfigStatusArray = StaticArray<UnitConfigStatus, cUnitConfigStatusCount>;

/**
 * Unit node information.
 */
struct UnitNodeInfo : public NodeInfo {
    ResourceInfoArray mResources;
    RuntimeInfoArray  mRuntimes;

    /**
     * Compares unit node info.
     *
     * @param rhs unit node info to compare with.
     * @return bool.
     */
    bool operator==(const UnitNodeInfo& rhs) const
    {
        return NodeInfo::operator==(rhs) && mResources == rhs.mResources && mRuntimes == rhs.mRuntimes;
    }

    /**
     * Compares unit node info.
     *
     * @param rhs unit node info to compare with.
     * @return bool.
     */
    bool operator!=(const UnitNodeInfo& rhs) const { return !operator==(rhs); }
};

using UnitNodeInfoArray = StaticArray<UnitNodeInfo, cMaxNumNodes>;

/**
 * Update item status.
 */
struct UpdateItemStatus {
    StaticString<cIDLen>      mItemID;
    StaticString<cVersionLen> mVersion;
    ImageStatusArray          mStatuses;

    /**
     * Compares update item status.
     *
     * @param rhs update item status to compare with.
     * @return bool.
     */
    bool operator==(const UpdateItemStatus& rhs) const
    {
        return mItemID == rhs.mItemID && mVersion == rhs.mVersion && mStatuses == rhs.mStatuses;
    }

    /**
     * Compares update item status.
     *
     * @param rhs update item status to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateItemStatus& rhs) const { return !operator==(rhs); }
};

using UpdateItemStatusArray = StaticArray<UpdateItemStatus, cMaxNumUpdateItems>;

/**
 * Unit instance status.
 */
struct UnitInstanceStatus : public InstanceStatusData, public PlatformInfo {
    uint64_t mInstance {};

    /**
     * Compares instance status.
     *
     * @param rhs instance status to compare with.
     * @return bool.
     */
    bool operator==(const UnitInstanceStatus& rhs) const
    {
        return InstanceStatusData::operator==(rhs) && PlatformInfo::operator==(rhs) && mInstance == rhs.mInstance;
    }

    /**
     * Compares instance status.
     *
     * @param rhs instance status to compare with.
     * @return bool.
     */
    bool operator!=(const UnitInstanceStatus& rhs) const { return !operator==(rhs); }
};

using UnitInstanceStatusArray = StaticArray<UnitInstanceStatus, cMaxNumInstances>;

/**
 * Instances statuses.
 */
struct UnitInstancesStatuses {
    StaticString<cIDLen>      mItemID;
    StaticString<cIDLen>      mSubjectID;
    StaticString<cVersionLen> mVersion;
    UnitInstanceStatusArray   mInstances;

    /**
     * Compare instances statuses.
     *
     * @param rhs instances statuses to compare with.
     * @return bool.
     */
    bool operator==(const UnitInstancesStatuses& rhs) const
    {
        return mItemID == rhs.mItemID && mSubjectID == rhs.mSubjectID && mVersion == rhs.mVersion
            && mInstances == rhs.mInstances;
    }

    /**
     * Compares instances statuses.
     *
     * @param rhs instances statuses to compare with.
     * @return bool.
     */
    bool operator!=(const UnitInstancesStatuses& rhs) const { return !operator==(rhs); }
};

using UnitInstancesStatusesArray = StaticArray<UnitInstancesStatuses, cMaxNumUpdateItems>;

/**
 * Unit status
 */
struct UnitStatus {
    bool                                 mIsDeltaInfo {};
    Optional<UnitConfigStatusArray>      mUnitConfig;
    Optional<UnitNodeInfoArray>          mNodes;
    Optional<UpdateItemStatusArray>      mUpdateItems;
    Optional<UnitInstancesStatusesArray> mInstances;
    Optional<SubjectArray>               mUnitSubjects;

    /**
     * Compares unit status.
     *
     * @param rhs unit status to compare with.
     * @return bool.
     */
    bool operator==(const UnitStatus& rhs) const
    {
        return mUnitConfig == rhs.mUnitConfig && mNodes == rhs.mNodes && mUnitSubjects == rhs.mUnitSubjects
            && mUpdateItems == rhs.mUpdateItems;
    }

    /**
     * Compares unit status.
     *
     * @param rhs unit status to compare with.
     * @return bool.
     */
    bool operator!=(const UnitStatus& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
