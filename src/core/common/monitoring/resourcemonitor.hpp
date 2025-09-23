/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_RESOURCEMONITOR_HPP_
#define AOS_CORE_COMMON_MONITORING_RESOURCEMONITOR_HPP_

#include <core/common/tools/memory.hpp>
#include <core/common/tools/timer.hpp>
#include <core/sm/resourcemanager/resourcemanager.hpp>

#include "alertprocessor.hpp"
#include "average.hpp"
#include "monitoring.hpp"

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
    Error Init(const Config& config, const iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
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
     * Updates instance's run state.
     *
     * @param instanceID instance ID.
     * @param state state.
     * @return Error.
     */
    Error UpdateInstanceState(const String& instanceID, InstanceState state) override;

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
    static constexpr auto cAllocatorSize
        = Max(sizeof(NodeInfoObsolete) + sizeof(sm::resourcemanager::NodeConfig) + sizeof(ResourceIdentifier),
            sizeof(InstanceMonitoringData) + sizeof(AlertProcessorArray) + sizeof(ResourceIdentifier));

    String       GetParameterName(const ResourceIdentifier& id) const;
    AlertVariant CreateSystemQuotaAlertTemplate(const ResourceIdentifier& resourceIdentifier) const;
    AlertVariant CreateInstanceQuotaAlertTemplate(
        const InstanceIdent& instanceIdent, const ResourceIdentifier& resourceIdentifier) const;
    double CPUToDMIPs(double cpuPersentage) const;

    Error SetupAlerts(
        const ResourceIdentifier identifierTemplate, const AlertRules& rules, Array<AlertProcessor>& alertProcessors);

    Error SetupSystemAlerts(const sm::resourcemanager::NodeConfig& nodeConfig);
    Error SetupInstanceAlerts(const String& instanceID, const InstanceMonitorParams& instanceParams);
    void  NormalizeMonitoringData();
    void  ProcessMonitoring();
    void  ProcessAlerts(const MonitoringData& monitoringData, const Time& time, Array<AlertProcessor>& alertProcessors);
    RetWithError<uint64_t> GetCurrentUsage(const ResourceIdentifier& id, const MonitoringData& monitoringData) const;
    RetWithError<uint64_t> GetPartitionTotalSize(const String& name) const;
    UniquePtr<ResourceIdentifier> CreateResourceIdentifier(ResourceLevel level, ResourceType type,
        const Optional<StaticString<cPartitionNameLen>>& partitionName = {},
        const Optional<StaticString<cIDLen>>&            instanceID    = {}) const;

    Config                                   mConfig;
    ResourceUsageProviderItf*                mResourceUsageProvider {};
    sm::resourcemanager::ResourceManagerItf* mResourceManager {};
    SenderItf*                               mMonitorSender {};
    alerts::SenderItf*                       mAlertSender = {};
    ConnectionPublisherItf*                  mConnectionPublisher {};

    Average mAverage;

    NodeMonitoringData                                                        mNodeMonitoringData {};
    StaticMap<StaticString<cIDLen>, InstanceMonitoringData, cMaxNumInstances> mInstanceMonitoringData;

    bool mSendMonitoring {};

    Mutex mMutex;
    Timer mTimer = {};

    size_t mMaxDMIPS {};
    size_t mMaxMemory {};

    AlertProcessorArray                                                    mAlertProcessors;
    StaticMap<StaticString<cIDLen>, AlertProcessorArray, cMaxNumInstances> mInstanceAlertProcessors;

    mutable StaticAllocator<cAllocatorSize> mAllocator;
};

} // namespace aos::monitoring

#endif
