/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_MONITORING_HPP_
#define AOS_MONITORING_HPP_

#include "aos/common/connectionsubsc.hpp"
#include "aos/common/monitoring/alertprocessor.hpp"
#include "aos/common/tools/error.hpp"
#include "aos/common/tools/thread.hpp"
#include "aos/common/types.hpp"
#include "aos/iam/nodeinfoprovider.hpp"

namespace aos::monitoring {

/**
 * Monitoring data.
 */
struct MonitoringData {
    double                   mCPU;
    size_t                   mRAM;
    PartitionInfoStaticArray mPartitions;
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

// Partition monitoring param.
struct PartitionParam {
    StaticString<cPartitionNameLen> mName;
    StaticString<cFilePathLen>      mPath;
};

/**
 * Instance resource monitor parameters.
 */
struct InstanceMonitorParams {
    InstanceIdent                                  mInstanceIdent;
    StaticArray<PartitionParam, cMaxNumPartitions> mPartitions;
    uint32_t                                       mUID;
    uint32_t                                       mGID;
    Optional<AlertRules>                           mAlertRules;
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
     */
    explicit InstanceMonitoringData(const InstanceIdent& instanceIdent)
        : mInstanceIdent(instanceIdent)
    {
    }

    /**
     * Constructs a new Instance Monitoring Data object.
     *
     * @param monitoringParams monitoring params.
     */
    explicit InstanceMonitoringData(const InstanceMonitorParams& monitoringParams)
        : mInstanceIdent(monitoringParams.mInstanceIdent)
        , mMonitoringData({0, 0, {}, 0, 0})
        , mUID(monitoringParams.mUID)
        , mGID(monitoringParams.mGID)
    {
        for (const auto& partition : monitoringParams.mPartitions) {
            PartitionInfo partitionInfo = {partition.mName, {}, partition.mPath, 0, 0};

            [[maybe_unused]] auto err = mMonitoringData.mPartitions.PushBack(partitionInfo);
            assert(err.IsNone());
        }
    }

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

    InstanceIdent    mInstanceIdent  = {};
    MonitoringData   mMonitoringData = {};
    uint32_t         mUID            = 0;
    uint32_t         mGID            = 0;
    InstanceRunState mRunState       = InstanceRunStateEnum::eFailed;

    /**
     * Compares instance monitoring data.
     *
     * @param data instance monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const InstanceMonitoringData& data) const
    {
        return mInstanceIdent == data.mInstanceIdent && mMonitoringData == data.mMonitoringData && mUID == data.mUID
            && mGID == data.mGID;
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
 * Resource usage provider interface.
 */
class ResourceUsageProviderItf {
public:
    /**
     * Destructor
     */
    virtual ~ResourceUsageProviderItf() = default;

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
     * @param[out] monitoringData instance monitoring data.
     * @return Error.
     */
    virtual Error GetInstanceMonitoringData(const String& instanceID, InstanceMonitoringData& monitoringData) = 0;
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
     * Updates instance's run state.
     *
     * @param instanceID instance ID.
     * @param runState run state.
     * @return Error.
     */
    virtual Error UpdateInstanceRunState(const String& instanceID, InstanceRunState runState) = 0;

    /**
     * Stops instance monitoring.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error StopInstanceMonitoring(const String& instanceID) = 0;

    /**
     * Returns average monitoring data.
     *
     * @param[out] monitoringData monitoring data.
     * @return Error.
     */
    virtual Error GetAverageMonitoringData(NodeMonitoringData& monitoringData) = 0;
};

} // namespace aos::monitoring

#endif
