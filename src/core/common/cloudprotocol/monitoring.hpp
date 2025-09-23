/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_MONITORING_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_MONITORING_HPP_

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

using PartitionUsageStaticArray = StaticArray<PartitionUsage, cMaxNumPartitions>;

/**
 * Monitoring data.
 */
struct MonitoringData {
    Time                      mTime;
    size_t                    mCPU {};
    size_t                    mRAM {};
    size_t                    mDownload {};
    size_t                    mUpload {};
    PartitionUsageStaticArray mPartitions;

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

using MonitoringDataStaticArray = StaticArray<MonitoringData, cMonitoringItemsCount>;

/**
 * Instance state information.
 */
struct InstanceStateInfo {
    Time          mTimestamp;
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

using InstanceStateInfoStaticArray = StaticArray<InstanceStateInfo, cMonitoringItemsCount>;

/**
 * Instance monitoring data.
 */
struct InstanceMonitoringData : public InstanceIdent {
    Identity                     mNodeID;
    MonitoringDataStaticArray    mItems;
    InstanceStateInfoStaticArray mStates;

    /**
     * Compares instance monitoring data.
     *
     * @param data instance monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const InstanceMonitoringData& data) const
    {
        return InstanceIdent::operator==(data) && mNodeID == data.mNodeID && mItems == data.mItems
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

using InstanceMonitoringDataStaticArray = StaticArray<InstanceMonitoringData, cMaxNumInstances>;

/**
 * Node state info.
 */
struct NodeStateInfo {
    Time      mTimestamp;
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

using NodeStateInfoStaticArray = StaticArray<NodeStateInfo, cMonitoringItemsCount>;

/**
 * Node monitoring data.
 */
struct NodeMonitoringData {
    Identity                  mNodeID;
    MonitoringDataStaticArray mItems;
    NodeStateInfoStaticArray  mStates;

    /**
     * Compares node monitoring data.
     *
     * @param data node monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const NodeMonitoringData& data) const
    {
        return mNodeID == data.mNodeID && mItems == data.mItems && mStates == data.mStates;
    }

    /**
     * Compares node monitoring data.
     *
     * @param data node monitoring data to compare with.
     * @return bool.
     */
    bool operator!=(const NodeMonitoringData& data) const { return !operator==(data); }
};

using NodeMonitoringDataStaticArray = StaticArray<NodeMonitoringData, cMaxNumNodes>;

/**
 * Monitoring message type.
 */
struct Monitoring {
    NodeMonitoringDataStaticArray     mNodes;
    InstanceMonitoringDataStaticArray mInstances;

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
