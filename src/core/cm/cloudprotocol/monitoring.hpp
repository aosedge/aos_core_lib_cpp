/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_CLOUDPROTOCOL_MONITORING_HPP_
#define AOS_CORE_CM_CLOUDPROTOCOL_MONITORING_HPP_

#include <core/common/tools/optional.hpp>
#include <core/common/types/types.hpp>

namespace aos::cloudprotocol {

/**
 * Monitoring items count.
 */
static constexpr auto cMonitoringItemsCount = AOS_CONFIG_CLOUDPROTOCOL_MONITORING_ITEMS_COUNT;

/**
 * Partition usage.
 */
struct PartitionUsage {
    StaticString<cPartitionNameLen> mName;
    size_t                          mUsedSize {};

    /**
     * Compares partition usages.
     * @param usage object to compare with.
     * @return bool.
     */
    bool operator==(const PartitionUsage& usage) const { return mName == usage.mName && mUsedSize == usage.mUsedSize; }

    /**
     * Compares partition usages.
     * @param usage object to compare with.
     * @return bool.
     */
    bool operator!=(const PartitionUsage& usage) const { return !operator==(usage); }
};

using PartitionUsageArray = StaticArray<PartitionUsage, cMaxNumPartitions>;

/**
 * Monitoring data.
 */
struct MonitoringData {
    Time                mTimestamp {};
    size_t              mCPU {};
    size_t              mRAM {};
    size_t              mDownload {};
    size_t              mUpload {};
    PartitionUsageArray mPartitions;

    /**
     * Compares monitoring data.
     *
     * @param data monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const MonitoringData& data) const
    {
        return mCPU == data.mCPU && mRAM == data.mRAM && mPartitions == data.mPartitions && mDownload == data.mDownload
            && mUpload == data.mUpload;
    }

    /**
     * Compares monitoring data.
     *
     * @param data monitoring data to compare with.
     * @return bool.
     */
    bool operator!=(const MonitoringData& data) const { return !operator==(data); }
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
     * @param info instance state info to compare with.
     * @return bool.
     */
    bool operator==(const InstanceStateInfo& info) const
    {
        return mTimestamp == info.mTimestamp && mState == info.mState;
    }

    /**
     * Compares instance state info.
     *
     * @param info instance state info to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceStateInfo& info) const { return !operator==(info); };
};

using InstanceStateInfoArray = StaticArray<InstanceStateInfo, cMonitoringItemsCount>;

/**
 * Instance monitoring data.
 */
struct InstanceMonitoringData : public InstanceIdent {
    Identity               mNode;
    MonitoringDataArray    mItems;
    InstanceStateInfoArray mStates;

    /**
     * Compares instance monitoring data.
     *
     * @param data instance monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const InstanceMonitoringData& data) const
    {
        return InstanceIdent::operator==(data) && mNode == data.mNode && mItems == data.mItems
            && mStates == data.mStates;
    }

    /**
     * Compares instance monitoring data.
     *
     * @param data instance monitoring data to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceMonitoringData& data) const { return !operator==(data); }
};

using InstanceMonitoringDataArray = StaticArray<InstanceMonitoringData, cMaxNumInstances>;

/**
 * Node state info.
 */
struct NodeStateInfo {
    Time      mTimestamp {};
    bool      mProvisioned = false;
    NodeState mState;

    /**
     * Compares node state info.
     *
     * @param info node state info to compare with.
     * @return bool.
     */
    bool operator==(const NodeStateInfo& info) const
    {
        return mTimestamp == info.mTimestamp && mProvisioned == info.mProvisioned && mState == info.mState;
    }

    /**
     * Compares node state info.
     *
     * @param info node state info to compare with.
     * @return bool.
     */
    bool operator!=(const NodeStateInfo& info) const { return !operator==(info); };
};

using NodeStateInfoArray = StaticArray<NodeStateInfo, cMonitoringItemsCount>;

/**
 * Node monitoring data.
 */
struct NodeMonitoringData {
    Identity            mNode;
    MonitoringDataArray mItems;
    NodeStateInfoArray  mStates;

    /**
     * Compares node monitoring data.
     *
     * @param data node monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const NodeMonitoringData& data) const
    {
        return mNode == data.mNode && mItems == data.mItems && mStates == data.mStates;
    }

    /**
     * Compares node monitoring data.
     *
     * @param data node monitoring data to compare with.
     * @return bool.
     */
    bool operator!=(const NodeMonitoringData& data) const { return !operator==(data); }
};

using NodeMonitoringDataArray = StaticArray<NodeMonitoringData, cMaxNumNodes>;

/**
 * Monitoring message type.
 */
struct Monitoring {
    NodeMonitoringDataArray     mNodes;
    InstanceMonitoringDataArray mInstances;

    /**
     * Compares monitoring message.
     *
     * @param monitoring monitoring message to compare with.
     * @return bool.
     */
    bool operator==(const Monitoring& monitoring) const
    {
        return mNodes == monitoring.mNodes && mInstances == monitoring.mInstances;
    }

    /**
     * Compares monitoring message.
     *
     * @param monitoring monitoring message to compare with.
     * @return bool.
     */
    bool operator!=(const Monitoring& monitoring) const { return !operator==(monitoring); }
};

} // namespace aos::cloudprotocol

#endif
