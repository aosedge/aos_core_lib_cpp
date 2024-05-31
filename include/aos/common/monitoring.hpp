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
#include "aos/common/types.hpp"
#include <aos/common/tools/thread.hpp>
#include <aos/common/tools/timer.hpp>

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
     * Constructs a new Instance Monitoring Data object
     */
    InstanceMonitoringData() = default;

    /**
     * Constructs a new Instance Monitoring Data object
     *
     * @param instanceID instance ID
     * @param monitoringData monitoring data
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
    MonitoringData                                        mMonitoringData;
    timespec                                              mTimestamp;
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
     * Initializes resource usage provider
     *
     * @return Error
     */
    virtual Error Init() = 0;

    /**
     * Returns node info
     *
     * @param systemInfo system info
     * @return Error
     */
    virtual Error GetNodeInfo(NodeInfo& systemInfo) const = 0;

    /**
     * Returns resource config
     *
     * @param nodeID node ident
     * @param monitoringData monitoring data
     * @return Error
     */
    virtual Error GetNodeMonitoringData(const String& nodeID, MonitoringData& monitoringData) = 0;

    /**
     * Gatherings instance monitoring data
     *
     * @param instance instance ident
     * @param monitoringData monitoring data
     * @return Error
     */
    virtual Error GetInstanceMonitoringData(const String& instanceID, MonitoringData& monitoringData) = 0;
};

/**
 * Monitor sender interface.
 */
class SenderItf {
public:
    /**
     * Sends monitoring data
     *
     * @param monitoringData monitoring data
     * @return Error
     */
    virtual Error SendMonitoringData(const NodeMonitoringData& monitoringData) = 0;
};

/**
 * Resource monitor interface.
 */
class ResourceMonitorItf {
public:
    /**
     * Destructor
     */
    virtual ~ResourceMonitorItf() = default;

    /**
     * Gets the node info object
     *
     * @param nodeInfo node info
     * @return Error
     */
    virtual Error GetNodeInfo(NodeInfo& nodeInfo) const = 0;

    /**
     * Starts instance monitoring
     *
     * @param instanceID instance ident
     * @param monitoringConfig monitoring config
     * @return Error
     */
    virtual Error StartInstanceMonitoring(const String& instanceID, const InstanceMonitorParams& monitoringConfig) = 0;

    /**
     * Stops instance monitoring
     *
     * @param instanceID instance ident
     * @return Error
     */
    virtual Error StopInstanceMonitoring(const String& instanceID) = 0;
};

/**
 * Resource monitor.
 */
class ResourceMonitor : public ResourceMonitorItf, public ConnectionSubscriberItf {
public:
    /**
     * Constructor
     */
    ResourceMonitor() = default;

    /**
     * Destructor
     */
    ~ResourceMonitor();

    /**
     * Initializes resource monitor
     *
     * @param resourceUsageProvider resource usage provider
     * @param monitorSender monitor sender
     * @return Error
     */
    Error Init(ResourceUsageProviderItf& resourceUsageProvider, SenderItf& monitorSender,
        ConnectionPublisherItf& connectionPublisher);

    /**
     * Gets the node info object
     *
     * @param nodeInfo node info
     * @return Error
     */
    Error GetNodeInfo(NodeInfo& nodeInfo) const override;

    /**
     * Starts instance monitoring
     *
     * @param instanceID instance ident
     * @param monitoringConfig monitoring config
     * @return Error
     */
    Error StartInstanceMonitoring(const String& instanceID, const InstanceMonitorParams& monitoringConfig) override;

    /**
     * Stops instance monitoring
     *
     * @param instanceID instance ident
     * @return Error
     */
    Error StopInstanceMonitoring(const String& instanceID) override;

    /**
     * Respond to a connection event.
     */
    void OnConnect() override;

    /**
     * Respond to a disconnection event.
     */
    void OnDisconnect() override;

private:
    static constexpr auto cTimeoutSend = AOS_CONFIG_MONITORING_SEND_PERIOD_SEC;
    static constexpr auto cPollPeriod  = AOS_CONFIG_MONITORING_POLL_PERIOD_SEC;

    ResourceUsageProviderItf*    mResourceUsageProvider {};
    SenderItf*                   mMonitorSender {};
    aos::ConnectionPublisherItf* mConnectionPublisher {};
    NodeMonitoringData           mNodeMonitoringData {};
    Mutex                        mMutex;
    bool                         mFinishMonitoring {};
    bool                         mSendMonitoring {};
    Thread<>                     mThreadMonitoring     = {};
    Thread<>                     mThreadSendMonitoring = {};

    Error RunGatheringNodeMonitoringData();
    Error RunSendMonitoringData();
};

} // namespace monitoring
} // namespace aos

#endif
