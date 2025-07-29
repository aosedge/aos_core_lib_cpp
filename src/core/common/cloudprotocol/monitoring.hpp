/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_MONITORING_HPP_
#define AOS_CLOUDPROTOCOL_MONITORING_HPP_

#include <core/common/crypto/crypto.hpp>
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

/**
 * Node monitoring data.
 */
struct NodeMonitoringData {
    StaticString<cNodeIDLen>                           mNodeID;
    StaticArray<MonitoringData, cMonitoringItemsCount> mItems;

    /**
     * Compares node monitoring data.
     *
     * @param data node monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const NodeMonitoringData& data) const { return mNodeID == data.mNodeID && mItems == data.mItems; }

    /**
     * Compares node monitoring data.
     *
     * @param data node monitoring data to compare with.
     * @return bool.
     */
    bool operator!=(const NodeMonitoringData& data) const { return !operator==(data); }
};

/**
 * Instance monitoring data.
 */
struct InstanceMonitoringData {
    InstanceIdent                                      mInstanceIdent;
    StaticString<cNodeIDLen>                           mNodeID;
    StaticArray<MonitoringData, cMonitoringItemsCount> mItems;

    /**
     * Compares instance monitoring data.
     *
     * @param data instance monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const InstanceMonitoringData& data) const
    {
        return mInstanceIdent == data.mInstanceIdent && mNodeID == data.mNodeID && mItems == data.mItems;
    }

    /**
     * Compares instance monitoring data.
     *
     * @param data instance monitoring data to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceMonitoringData& data) const { return !operator==(data); }
};

/**
 * Monitoring message type.
 */
struct Monitoring {
    StaticArray<NodeMonitoringData, cMaxNumNodes>         mNodes;
    StaticArray<InstanceMonitoringData, cMaxNumInstances> mServiceInstances;

    /**
     * Compares monitoring message.
     *
     * @param monitoring monitoring message to compare with.
     * @return bool.
     */
    bool operator==(const Monitoring& monitoring) const
    {
        return mNodes == monitoring.mNodes && mServiceInstances == monitoring.mServiceInstances;
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
