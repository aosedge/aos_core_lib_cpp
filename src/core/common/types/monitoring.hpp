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
    friend bool operator==(const InstanceMonitoringParams& lhs, const InstanceMonitoringParams& rhs)
    {
        return lhs.mAlertRules == rhs.mAlertRules;
    };

    /**
     * Compares instance monitoring parameters.
     *
     * @param rhs instance monitoring parameters to compare with.
     * @return bool.
     */
    friend bool operator!=(const InstanceMonitoringParams& lhs, const InstanceMonitoringParams& rhs)
    {
        return !(lhs == rhs);
    };
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
    friend bool operator==(const PartitionUsage& lhs, const PartitionUsage& rhs)
    {
        return lhs.mName == rhs.mName && lhs.mUsedSize == rhs.mUsedSize;
    };

    /**
     * Compares partition usages.
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const PartitionUsage& lhs, const PartitionUsage& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const MonitoringData& lhs, const MonitoringData& rhs)
    {
        return lhs.mCPU == rhs.mCPU && lhs.mRAM == rhs.mRAM && lhs.mPartitions == rhs.mPartitions
            && lhs.mDownload == rhs.mDownload && lhs.mUpload == rhs.mUpload;
    };

    /**
     * Compares monitoring data.
     *
     * @param rhs monitoring data to compare with.
     * @return bool.
     */
    friend bool operator!=(const MonitoringData& lhs, const MonitoringData& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const InstanceStateInfo& lhs, const InstanceStateInfo& rhs)
    {
        return lhs.mTimestamp == rhs.mTimestamp && lhs.mState == rhs.mState;
    };

    /**
     * Compares instance state info.
     *
     * @param rhs instance state info to compare with.
     * @return bool.
     */
    friend bool operator!=(const InstanceStateInfo& lhs, const InstanceStateInfo& info) { return !(lhs == info); };
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
    friend bool operator==(const InstanceMonitoringData& lhs, const InstanceMonitoringData& rhs)
    {
        return (static_cast<const InstanceIdent&>(lhs) == rhs) && lhs.mNodeID == rhs.mNodeID && lhs.mItems == rhs.mItems
            && lhs.mStates == rhs.mStates;
    };

    /**
     * Compares instance monitoring data.
     *
     * @param data instance monitoring data to compare with.
     * @return bool.
     */
    friend bool operator!=(const InstanceMonitoringData& lhs, const InstanceMonitoringData& rhs)
    {
        return !(lhs == rhs);
    };
};

using InstanceMonitoringDataArray = StaticArray<InstanceMonitoringData, cMaxNumInstances>;

/**
 * Node state info.
 */
struct NodeStateInfo {
    Time      mTimestamp {};
    NodeState mState;
    bool      mIsConnected {};

    /**
     * Compares node state info.
     *
     * @param rhs node state info to compare with.
     * @return bool.
     */
    friend bool operator==(const NodeStateInfo& lhs, const NodeStateInfo& rhs)
    {
        return lhs.mTimestamp == rhs.mTimestamp && lhs.mState == rhs.mState && lhs.mIsConnected == rhs.mIsConnected;
    };

    /**
     * Compares node state info.
     *
     * @param rhs node state info to compare with.
     * @return bool.
     */
    friend bool operator!=(const NodeStateInfo& lhs, const NodeStateInfo& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const NodeMonitoringData& lhs, const NodeMonitoringData& rhs)
    {
        return lhs.mNodeID == rhs.mNodeID && lhs.mItems == rhs.mItems && lhs.mStates == rhs.mStates;
    };

    /**
     * Compares node monitoring data.
     *
     * @param rhs node monitoring data to compare with.
     * @return bool.
     */
    friend bool operator!=(const NodeMonitoringData& lhs, const NodeMonitoringData& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const Monitoring& lhs, const Monitoring& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mNodes == rhs.mNodes
            && lhs.mInstances == rhs.mInstances;
    };

    /**
     * Compares monitoring message.
     *
     * @param rhs monitoring message to compare with.
     * @return bool.
     */
    friend bool operator!=(const Monitoring& lhs, const Monitoring& rhs) { return !(lhs == rhs); };
};

} // namespace aos

#endif
