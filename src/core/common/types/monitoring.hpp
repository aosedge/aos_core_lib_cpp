/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_MONITORING_HPP_
#define AOS_CORE_COMMON_TYPES_MONITORING_HPP_

#include "common.hpp"

namespace aos {

/**
 * Monitoring items count.
 */
static constexpr auto cMonitoringItemsCount = AOS_CONFIG_TYPES_MONITORING_ITEMS_COUNT;

/**
 * Instance monitoring parameters.
 */
struct InstanceMonitoringParams {
    Optional<AlertRules> mAlertRules;

    /**
     * Compares instance monitoring parameters.
     *
     * @param rhs instance monitoring parameters to compare with.
     * @return bool.
     */
    bool operator==(const InstanceMonitoringParams& rhs) const { return mAlertRules == rhs.mAlertRules; }

    /**
     * Compares instance monitoring parameters.
     *
     * @param rhs instance monitoring parameters to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceMonitoringParams& rhs) const { return !operator==(rhs); }
};

/**
 * Partition usage.
 */
struct PartitionUsage {
    StaticString<cPartitionNameLen> mName;
    size_t                          mUsedSize {};

    /**
     * Compares partition usages.
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const PartitionUsage& rhs) const { return mName == rhs.mName && mUsedSize == rhs.mUsedSize; }

    /**
     * Compares partition usages.
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const PartitionUsage& rhs) const { return !operator==(rhs); }
};

using PartitionUsageArray = StaticArray<PartitionUsage, cMaxNumPartitions>;

/**
 * Monitoring data.
 */
struct MonitoringData {
    Time                mTimestamp {};
    double              mCPU {};
    size_t              mRAM {};
    PartitionUsageArray mPartitions;
    size_t              mDownload {};
    size_t              mUpload {};

    /**
     * Compares monitoring data.
     *
     * @param rhs monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const MonitoringData& rhs) const
    {
        return mCPU == rhs.mCPU && mRAM == rhs.mRAM && mPartitions == rhs.mPartitions && mDownload == rhs.mDownload
            && mUpload == rhs.mUpload;
    }

    /**
     * Compares monitoring data.
     *
     * @param rhs monitoring data to compare with.
     * @return bool.
     */
    bool operator!=(const MonitoringData& rhs) const { return !operator==(rhs); }
};

using MonitoringDataArray = StaticArray<MonitoringData, cMonitoringItemsCount>;

/**
 * Instance state information.
 */
struct InstanceStateInfo {
    Time          mTimestamp {};
    InstanceState mState;

    /**
     * Compares instance state info.
     *
     * @param rhs instance state info to compare with.
     * @return bool.
     */
    bool operator==(const InstanceStateInfo& rhs) const { return mTimestamp == rhs.mTimestamp && mState == rhs.mState; }

    /**
     * Compares instance state info.
     *
     * @param rhs instance state info to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceStateInfo& info) const { return !operator==(info); };
};

using InstanceStateInfoArray = StaticArray<InstanceStateInfo, cMonitoringItemsCount>;

/**
 * Instance monitoring data.
 */
struct InstanceMonitoringData : public InstanceIdent {
    StaticString<cIDLen>   mNodeID;
    MonitoringDataArray    mItems;
    InstanceStateInfoArray mStates;

    /**
     * Compares instance monitoring data.
     *
     * @param rhs instance monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const InstanceMonitoringData& rhs) const
    {
        return InstanceIdent::operator==(rhs) && mNodeID == rhs.mNodeID && mItems == rhs.mItems
            && mStates == rhs.mStates;
    }

    /**
     * Compares instance monitoring data.
     *
     * @param data instance monitoring data to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceMonitoringData& rhs) const { return !operator==(rhs); }
};

using InstanceMonitoringDataArray = StaticArray<InstanceMonitoringData, cMaxNumInstances>;

/**
 * Node state info.
 */
struct NodeStateInfo {
    Time      mTimestamp {};
    bool      mProvisioned {};
    NodeState mState;

    /**
     * Compares node state info.
     *
     * @param rhs node state info to compare with.
     * @return bool.
     */
    bool operator==(const NodeStateInfo& rhs) const
    {
        return mTimestamp == rhs.mTimestamp && mProvisioned == rhs.mProvisioned && mState == rhs.mState;
    }

    /**
     * Compares node state info.
     *
     * @param rhs node state info to compare with.
     * @return bool.
     */
    bool operator!=(const NodeStateInfo& rhs) const { return !operator==(rhs); };
};

using NodeStateInfoArray = StaticArray<NodeStateInfo, cMonitoringItemsCount>;

/**
 * Node monitoring data.
 */
struct NodeMonitoringData {
    StaticString<cIDLen> mNodeID;
    MonitoringDataArray  mItems;
    NodeStateInfoArray   mStates;

    /**
     * Compares node monitoring data.
     *
     * @param rhs node monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const NodeMonitoringData& rhs) const
    {
        return mNodeID == rhs.mNodeID && mItems == rhs.mItems && mStates == rhs.mStates;
    }

    /**
     * Compares node monitoring data.
     *
     * @param rhs node monitoring data to compare with.
     * @return bool.
     */
    bool operator!=(const NodeMonitoringData& rhs) const { return !operator==(rhs); }
};

using NodeMonitoringDataArray = StaticArray<NodeMonitoringData, cMaxNumNodes>;

/**
 * Monitoring message type.
 */
struct Monitoring : public Protocol {
    NodeMonitoringDataArray     mNodes;
    InstanceMonitoringDataArray mInstances;

    /**
     * Compares monitoring message.
     *
     * @param rhs monitoring message to compare with.
     * @return bool.
     */
    bool operator==(const Monitoring& rhs) const
    {
        return Protocol::operator==(rhs) && mNodes == rhs.mNodes && mInstances == rhs.mInstances;
    }

    /**
     * Compares monitoring message.
     *
     * @param rhs monitoring message to compare with.
     * @return bool.
     */
    bool operator!=(const Monitoring& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
