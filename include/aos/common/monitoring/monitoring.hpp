/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_MONITORING_HPP_
#define AOS_MONITORING_HPP_

#include "aos/common/connectionsubsc.hpp"
#include "aos/common/tools/error.hpp"
#include "aos/common/tools/thread.hpp"
#include "aos/common/types.hpp"
#include "aos/iam/nodeinfoprovider.hpp"

namespace aos::monitoring {

/**
 * Monitoring data.
 */
struct MonitoringData {
    size_t                   mCPU;
    size_t                   mRAM;
    PartitionInfoStaticArray mDisk;
    uint64_t                 mDownload;
    uint64_t                 mUpload;

    /**
     * Compares monitoring data.
     *
     * @param data monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const MonitoringData& data) const
    {
        return mCPU == data.mCPU && mRAM == data.mRAM && mDisk == data.mDisk && mDownload == data.mDownload
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
 * Instance monitoring data.
 */
struct InstanceMonitoringData {
    /**
     * Constructs a new Instance Monitoring Data object.
     */
    InstanceMonitoringData() = default;

    /**
     * Constructs a new Instance Monitoring Data object.
     *
     * @param instanceIdent instance ident.
     * @param monitoringData monitoring data.
     */
    InstanceMonitoringData(const InstanceIdent& instanceIdent, const MonitoringData& monitoringData)
        : mInstanceIdent(instanceIdent)
        , mMonitoringData(monitoringData)
    {
    }

    InstanceIdent  mInstanceIdent;
    MonitoringData mMonitoringData;

    /**
     * Compares instance monitoring data.
     *
     * @param data instance monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const InstanceMonitoringData& data) const
    {
        return mInstanceIdent == data.mInstanceIdent && mMonitoringData == data.mMonitoringData;
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
 * Node monitoring data.
 */
struct NodeMonitoringData {
    StaticString<cNodeIDLen>                              mNodeID;
    Time                                                  mTimestamp;
    MonitoringData                                        mMonitoringData;
    StaticArray<InstanceMonitoringData, cMaxNumInstances> mServiceInstances;

    /**
     * Compares node monitoring data.
     *
     * @param data node monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const NodeMonitoringData& data) const
    {
        return mNodeID == data.mNodeID && mMonitoringData == data.mMonitoringData && mTimestamp == data.mTimestamp
            && mServiceInstances == data.mServiceInstances;
    }

    /**
     * Compares node monitoring data.
     *
     * @param data node monitoring data to compare with.
     * @return bool.
     */
    bool operator!=(const NodeMonitoringData& data) const { return !operator==(data); }
};

/**
 * Instance resource monitor parameters.
 */
struct InstanceMonitorParams {
    InstanceIdent            mInstanceIdent;
    PartitionInfoStaticArray mPartitions;
};

/**
 * Resource usage provider interface.
 */
class ResourceUsageProviderItf {
public:
    /**
     * Destructor
     */
    virtual ~ResourceUsageProviderItf() = default;

    /**
     * Initializes resource usage provider.
     *
     * @return Error.
     */
    virtual Error Init() = 0;

    /**
     * Returns node monitoring data.
     *
     * @param nodeID node ident.
     * @param[out] monitoringData monitoring data.
     * @return Error.
     */
    virtual Error GetNodeMonitoringData(const String& nodeID, MonitoringData& monitoringData) = 0;

    /**
     * Returns instance monitoring data.
     *
     * @param instanceID instance ID.
     * @param[out] monitoringData monitoring data.
     * @return Error.
     */
    virtual Error GetInstanceMonitoringData(const String& instanceID, MonitoringData& monitoringData) = 0;
};

/**
 * Monitor sender interface.
 */
class SenderItf {
public:
    /**
     * Sends monitoring data.
     *
     * @param monitoringData monitoring data.
     * @return Error.
     */
    virtual Error SendMonitoringData(const NodeMonitoringData& monitoringData) = 0;
};

/**
 * Resource monitor interface.
 */
class ResourceMonitorItf {
public:
    /**
     * Destructor.
     */
    virtual ~ResourceMonitorItf() = default;

    /**
     * Starts instance monitoring.
     *
     * @param instanceID instance ID.
     * @param monitoringConfig monitoring config.
     * @return Error.
     */
    virtual Error StartInstanceMonitoring(const String& instanceID, const InstanceMonitorParams& monitoringConfig) = 0;

    /**
     * Stops instance monitoring.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error StopInstanceMonitoring(const String& instanceID) = 0;
};

} // namespace aos::monitoring

#endif
