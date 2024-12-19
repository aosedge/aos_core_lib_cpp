/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_RESOURCEMONITOR_HPP_
#define AOS_RESOURCEMONITOR_HPP_

#include "aos/common/monitoring/alertprocessor.hpp"
#include "aos/common/monitoring/average.hpp"
#include "aos/common/monitoring/monitoring.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/sm/resourcemanager.hpp"

namespace aos::monitoring {

/**
 * Resource monitor config.
 */
struct Config {
    Duration mPollPeriod    = AOS_CONFIG_MONITORING_POLL_PERIOD_SEC * Time::cSeconds;
    Duration mAverageWindow = AOS_CONFIG_MONITORING_AVERAGE_WINDOW_SEC * Time::cSeconds;
};

/**
 * Resource monitor.
 */
class ResourceMonitor : public ResourceMonitorItf,
                        public ConnectionSubscriberItf,
                        public sm::resourcemanager::NodeConfigReceiverItf,
                        private NonCopyable {
public:
    /**
     * Initializes resource monitor.
     *
     * @param config config.
     * @param nodeInfoProvider node info provider.
     * @param resourceManager resource manager.
     * @param resourceUsageProvider resource usage provider.
     * @param monitorSender monitor sender.
     * @param alertSender alert sender.
     * @param connectionPublisher connection publisher.
     * @return Error.
     */
    Error Init(const Config& config, iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
        sm::resourcemanager::ResourceManagerItf& resourceManager, ResourceUsageProviderItf& resourceUsageProvider,
        SenderItf& monitorSender, alerts::SenderItf& alertSender, ConnectionPublisherItf& connectionPublisher);

    /**
     * Starts monitoring.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops monitoring.
     *
     * @return Error.
     */
    Error Stop();

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

    /**
     * Receives node config.
     *
     * @param nodeConfig node config.
     * @return Error.
     */
    Error ReceiveNodeConfig(const sm::resourcemanager::NodeConfig& nodeConfig) override;

private:
    String                      GetParameterName(const ResourceIdentifier& id) const;
    cloudprotocol::AlertVariant CreateSystemQuotaAlertTemplate(const ResourceIdentifier& resourceIdentifier) const;
    cloudprotocol::AlertVariant CreateInstanceQuotaAlertTemplate(
        const InstanceIdent& instanceIdent, const ResourceIdentifier& resourceIdentifier) const;
    double CPUToDMIPs(double cpuPersentage) const;

    Error SetupAlerts(
        const ResourceIdentifier identifierTemplate, const AlertRules& rules, Array<AlertProcessor>& alertProcessors);

    Error SetupSystemAlerts(const NodeConfig& nodeConfig);
    Error SetupInstanceAlerts(const String& instanceID, const InstanceMonitorParams& instanceParams);
    void  ProcessMonitoring();
    void  ProcessAlerts(const MonitoringData& monitoringData, const Time& time, Array<AlertProcessor>& alertProcessors);
    RetWithError<uint64_t> GetCurrentUsage(const ResourceIdentifier& id, const MonitoringData& monitoringData) const;
    RetWithError<uint64_t> GetPartitionTotalSize(const String& name) const;

    Config                                   mConfig;
    ResourceUsageProviderItf*                mResourceUsageProvider {};
    sm::resourcemanager::ResourceManagerItf* mResourceManager {};
    SenderItf*                               mMonitorSender {};
    alerts::SenderItf*                       mAlertSender = {};
    ConnectionPublisherItf*                  mConnectionPublisher {};

    Average mAverage;

    NodeMonitoringData                                                                mNodeMonitoringData {};
    StaticMap<StaticString<cInstanceIDLen>, InstanceMonitoringData, cMaxNumInstances> mInstanceMonitoringData;

    bool mFinishMonitoring {};
    bool mSendMonitoring {};

    Mutex               mMutex;
    ConditionalVariable mCondVar;
    Thread<>            mThread = {};

    uint64_t mMaxDMIPS {};
    uint64_t mMaxMemory {};

    AlertProcessorStaticArray                                                            mAlertProcessors;
    StaticMap<StaticString<cInstanceIDLen>, AlertProcessorStaticArray, cMaxNumInstances> mInstanceAlertProcessors;

    mutable StaticAllocator<sizeof(NodeInfo) + sizeof(sm::resourcemanager::NodeConfig)> mAllocator;
};

} // namespace aos::monitoring

#endif
