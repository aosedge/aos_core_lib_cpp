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

namespace aos {
namespace monitoring {

/**
 * Monitoring data.
 */
struct MonitoringData {
    size_t                                        mRAM;
    size_t                                        mCPU;
    StaticArray<PartitionInfo, cMaxNumPartitions> mDisk;
    uint64_t                                      mInTraffic;
    uint64_t                                      mOutTraffic;

    /**
     * Compares monitoring data.
     *
     * @param data monitoring data to compare with.
     * @return bool.
     */
    bool operator==(const MonitoringData& data) const
    {
        return mRAM == data.mRAM && mCPU == data.mCPU && mDisk == data.mDisk && mInTraffic == data.mInTraffic
            && mOutTraffic == data.mOutTraffic;
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
     * @param instanceID instance ID.
     * @param instanceIdent instance ident.
     * @param monitoringData monitoring data.
     */
    InstanceMonitoringData(
        const String& instanceID, const InstanceIdent& instanceIdent, const MonitoringData& monitoringData)
        : mInstanceID(instanceID)
        , mInstanceIdent(instanceIdent)
        , mMonitoringData(monitoringData)
    {
    }

    StaticString<cInstanceIDLen> mInstanceID;
    InstanceIdent                mInstanceIdent;
    MonitoringData               mMonitoringData;
};

/**
 * Node monitoring data.
 */
struct NodeMonitoringData {
    StaticString<cNodeIDLen>                              mNodeID;
    timespec                                              mTimestamp;
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
        return mNodeID == data.mNodeID && mMonitoringData == data.mMonitoringData
            && mTimestamp.tv_nsec == data.mTimestamp.tv_nsec && mTimestamp.tv_sec == data.mTimestamp.tv_sec;
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
    InstanceIdent                                 mInstanceIdent;
    StaticArray<PartitionInfo, cMaxNumPartitions> mPartitions;
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
     * @param monitoringData monitoring data.
     * @return Error.
     */
    virtual Error GetNodeMonitoringData(const String& nodeID, MonitoringData& monitoringData) = 0;

    /**
     * Returns instance monitoring data.
     *
     * @param instanceID instance ID.
     * @param monitoringData monitoring data.
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

/**
 * Resource monitor.
 */
class ResourceMonitor : public ResourceMonitorItf, public ConnectionSubscriberItf {
public:
    /**
     * Constructor.
     */
    ResourceMonitor() = default;

    /**
     * Destructor.
     */
    ~ResourceMonitor();

    /**
     * Initializes resource monitor.
     *
     * @param nodeInfoProvider node info provider.
     * @param resourceUsageProvider resource usage provider.
     * @param monitorSender monitor sender.
     * @return Error.
     */
    Error Init(iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
        ResourceUsageProviderItf& resourceUsageProvider, SenderItf& monitorSender,
        ConnectionPublisherItf& connectionPublisher);

    /**
     * Starts instance monitoring.
     *
     * @param instanceID instance ID.
     * @param monitoringConfig monitoring config.
     * @return Error.
     */
    Error StartInstanceMonitoring(const String& instanceID, const InstanceMonitorParams& monitoringConfig) override;

    /**
     * Stops instance monitoring.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    Error StopInstanceMonitoring(const String& instanceID) override;

    /**
     * Responds to a connection event.
     */
    void OnConnect() override;

    /**
     * Responds to a disconnection event.
     */
    void OnDisconnect() override;

private:
    static constexpr auto cPollPeriod = AOS_CONFIG_MONITORING_POLL_PERIOD_SEC * Time::cSeconds;

    ResourceUsageProviderItf* mResourceUsageProvider {};
    SenderItf*                mMonitorSender {};
    ConnectionPublisherItf*   mConnectionPublisher {};

    NodeMonitoringData mNodeMonitoringData {};

    bool mFinishMonitoring {};
    bool mSendMonitoring {};

    Mutex               mMutex;
    ConditionalVariable mCondVar;
    Thread<>            mThread = {};

    void ProcessMonitoring();
};

} // namespace monitoring
} // namespace aos

#endif
