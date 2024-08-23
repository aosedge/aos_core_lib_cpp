/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_RESOURCEMONITOR_HPP_
#define AOS_RESOURCEMONITOR_HPP_

#include "aos/common/monitoring/average.hpp"
#include "aos/common/monitoring/monitoring.hpp"
#include "aos/common/tools/memory.hpp"

namespace aos::monitoring {

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
     * Returns average monitoring data.
     *
     * @param[out] monitoringData monitoring data.
     * @return Error.
     */
    Error GetAverageMonitoringData(NodeMonitoringData& monitoringData) override;

    /**
     * Responds to a connection event.
     */
    void OnConnect() override;

    /**
     * Responds to a disconnection event.
     */
    void OnDisconnect() override;

private:
    static constexpr auto cPollPeriod    = AOS_CONFIG_MONITORING_POLL_PERIOD_SEC * Time::cSeconds;
    static constexpr auto cAverageWindow = AOS_CONFIG_MONITORING_AVERAGE_WINDOW_SEC * Time::cSeconds;

    void ProcessMonitoring();

    ResourceUsageProviderItf* mResourceUsageProvider {};
    SenderItf*                mMonitorSender {};
    ConnectionPublisherItf*   mConnectionPublisher {};

    Average mAverage;

    NodeMonitoringData                                          mNodeMonitoringData {};
    StaticMap<String, InstanceMonitoringData, cMaxNumInstances> mInstanceMonitoringData;

    bool mFinishMonitoring {};
    bool mSendMonitoring {};

    Mutex               mMutex;
    ConditionalVariable mCondVar;
    Thread<>            mThread = {};

    uint64_t mMaxDMIPS;

    mutable StaticAllocator<sizeof(NodeInfo)> mAllocator;
};

} // namespace aos::monitoring

#endif
