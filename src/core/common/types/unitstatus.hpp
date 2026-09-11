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
    friend bool operator==(const UnitNodeInfo& lhs, const UnitNodeInfo& rhs)
    {
        return (static_cast<const NodeInfo&>(lhs) == rhs) && lhs.mResources == rhs.mResources
            && lhs.mRuntimes == rhs.mRuntimes;
    };

    /**
     * Compares unit node info.
     *
     * @param rhs unit node info to compare with.
     * @return bool.
     */
    friend bool operator!=(const UnitNodeInfo& lhs, const UnitNodeInfo& rhs) { return !(lhs == rhs); };
};

using UnitNodeInfoArray = StaticArray<UnitNodeInfo, cMaxNumNodes>;

/**
 * Update item status.
 */
struct UpdateItemStatus {
    StaticString<cIDLen>      mItemID;
    UpdateItemType            mType;
    StaticString<cVersionLen> mVersion;
    bool                      mPreinstalled {};
    ItemState                 mState;
    Error                     mError;

    /**
     * Compares update item status.
     *
     * @param rhs update item status to compare with.
     * @return bool.
     */
    friend bool operator==(const UpdateItemStatus& lhs, const UpdateItemStatus& rhs)
    {
        return lhs.mItemID == rhs.mItemID && lhs.mType == rhs.mType && lhs.mVersion == rhs.mVersion
            && lhs.mPreinstalled == rhs.mPreinstalled && lhs.mState == rhs.mState && lhs.mError == rhs.mError;
    };

    /**
     * Compares update item status.
     *
     * @param rhs update item status to compare with.
     * @return bool.
     */
    friend bool operator!=(const UpdateItemStatus& lhs, const UpdateItemStatus& rhs) { return !(lhs == rhs); };
};

// Multiplying by 2 to account for existing and new versions of update items.
using UpdateItemStatusArray = StaticArray<UpdateItemStatus, cMaxNumUpdateItems * 2>;

/**
 * Unit instance status.
 */
struct UnitInstanceStatus : public InstanceStatusData {
    uint64_t mInstance {};

    /**
     * Compares instance status.
     *
     * @param rhs instance status to compare with.
     * @return bool.
     */
    friend bool operator==(const UnitInstanceStatus& lhs, const UnitInstanceStatus& rhs)
    {
        return (static_cast<const InstanceStatusData&>(lhs) == rhs) && lhs.mInstance == rhs.mInstance;
    };

    /**
     * Compares instance status.
     *
     * @param rhs instance status to compare with.
     * @return bool.
     */
    friend bool operator!=(const UnitInstanceStatus& lhs, const UnitInstanceStatus& rhs) { return !(lhs == rhs); };
};

using UnitInstanceStatusArray = StaticArray<UnitInstanceStatus*, cMaxNumInstances>;

/**
 * Instances statuses.
 */
struct UnitInstancesStatuses {
    StaticString<cIDLen>      mItemID;
    UpdateItemType            mType;
    StaticString<cIDLen>      mSubjectID;
    StaticString<cVersionLen> mVersion;
    bool                      mPreinstalled {};
    UnitInstanceStatusArray   mInstances;

    /**
     * Creates default instances statuses.
     */
    UnitInstancesStatuses() = default;

    /**
     * Creates instances statuses.
     *
     * @param itemID    update item ID.
     * @param subjectID subject ID.
     * @param version   update item version.
     * @param preinstalled  preinstalled flag.
     */
    UnitInstancesStatuses(const String& itemID, const UpdateItemType& type, const String& subjectID,
        const String& version, bool preinstalled)
        : mItemID(itemID)
        , mType(type)
        , mSubjectID(subjectID)
        , mVersion(version)
        , mPreinstalled(preinstalled)
    {
    }

    /**
     * Compare instances statuses.
     *
     * @param rhs instances statuses to compare with.
     * @return bool.
     */
    friend bool operator==(const UnitInstancesStatuses& lhs, const UnitInstancesStatuses& rhs)
    {
        if (lhs.mInstances.Size() != rhs.mInstances.Size()) {
            return false;
        }

        for (size_t i = 0; i < lhs.mInstances.Size(); i++) {
            if (*lhs.mInstances[i] != *rhs.mInstances[i]) {
                return false;
            }
        }

        return lhs.mItemID == rhs.mItemID && lhs.mType == rhs.mType && lhs.mSubjectID == rhs.mSubjectID
            && lhs.mVersion == rhs.mVersion && lhs.mPreinstalled == rhs.mPreinstalled;
    };

    /**
     * Compares instances statuses.
     *
     * @param rhs instances statuses to compare with.
     * @return bool.
     */
    friend bool operator!=(const UnitInstancesStatuses& lhs, const UnitInstancesStatuses& rhs)
    {
        return !(lhs == rhs);
    };
};

using UnitInstancesStatusesArray = StaticArray<UnitInstancesStatuses, cMaxNumUpdateItems>;

/**
 * Unit status
 */
struct UnitStatus : public Protocol {
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
    friend bool operator==(const UnitStatus& lhs, const UnitStatus& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mIsDeltaInfo == rhs.mIsDeltaInfo
            && lhs.mUnitConfig == rhs.mUnitConfig && lhs.mNodes == rhs.mNodes && lhs.mUpdateItems == rhs.mUpdateItems
            && lhs.mInstances == rhs.mInstances && lhs.mUnitSubjects == rhs.mUnitSubjects;
    };

    /**
     * Compares unit status.
     *
     * @param rhs unit status to compare with.
     * @return bool.
     */
    friend bool operator!=(const UnitStatus& lhs, const UnitStatus& rhs) { return !(lhs == rhs); };
};

} // namespace aos

#endif
