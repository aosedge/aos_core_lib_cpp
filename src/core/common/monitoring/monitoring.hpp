/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_MONITORING_HPP_
#define AOS_CORE_COMMON_MONITORING_MONITORING_HPP_

#include <core/common/alerts/itf/sender.hpp>
#include <core/common/iamclient/itf/currentnodeinfoprovider.hpp>
#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>
#include <core/common/nodeconfig/itf/nodeconfigprovider.hpp>
#include <core/common/tools/allocator.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/timer.hpp>

#include "itf/instanceinfoprovider.hpp"
#include "itf/monitoring.hpp"
#include "itf/nodemonitoringprovider.hpp"
#include "itf/sender.hpp"

#include "alertprocessor.hpp"
#include "average.hpp"
#include "config.hpp"

namespace aos::monitoring {

/**
 * Monitoring implementation.
 */
class Monitoring : public MonitoringItf,
                   private nodeconfig::NodeConfigListenerItf,
                   private instancestatusprovider::ListenerItf {
public:
    /**
     * Initializes monitoring.
     *
     * @param config monitoring configuration.
     * @param nodeConfigProvider provides node configuration.
     * @param currentNodeInfoProvider provides current node information.
     * @param sender sends monitoring data.
     * @param alertSender sends alerts.
     * @param nodeMonitoringProvider provides node monitoring data.
     * @param instanceInfoProvider provides instance information and monitoring data, optional.
     * @return Error.
     */
    Error Init(const Config& config, nodeconfig::NodeConfigProviderItf& nodeConfigProvider,
        iamclient::CurrentNodeInfoProviderItf& currentNodeInfoProvider, SenderItf& sender,
        alerts::SenderItf& alertSender, NodeMonitoringProviderItf& nodeMonitoringProvider,
        InstanceInfoProviderItf* instanceInfoProvider);

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
     * Returns average monitoring data.
     *
     * @param[out] monitoringData monitoring data.
     * @return Error.
     */
    Error GetAverageMonitoringData(NodeMonitoringData& monitoringData) override;

private:
    static constexpr auto cAllocatorSize = Max(sizeof(NodeMonitoringData), sizeof(InstanceStatusArray));

    struct Instance {
        InstanceIdent                      mIdent;
        AlertProcessorArray                mAlertProcessors;
        Optional<InstanceMonitoringParams> mMonitoringParams;
    };

    Error OnNodeConfigChanged(const NodeConfig& nodeConfig) override;
    void  OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses) override;
    void  HandleInstanceStatuses(const Array<InstanceStatus>& statuses);
    void  StartWatchingInstance(const InstanceStatus& instanceStatus);
    void  StopWatchingInstance(const InstanceStatus& instanceStatus);

    void  ProcessMonitoring();
    void  GetInstanceMonitoringData(Array<InstanceMonitoringData>& instanceMonitoringData);
    void  ProcessAlerts(NodeMonitoringData& monitoringData);
    void  ProcessAlerts(MonitoringData& monitoringData, AlertProcessorArray& alertProcessors);
    Error CreateAlertTemplate(const ResourceIdentifier& resourceIdentifier, AlertVariant& alert) const;
    Error AddAlertProcessor(
        const AlertRulePoints& rule, const ResourceIdentifier& identifier, Array<AlertProcessor>& processors);
    Error SetNodeAlertProcessors(const Optional<AlertRules>& alertRules);
    Error SetInstanceAlertProcessors(const Optional<InstanceMonitoringParams>& params, Instance& instance);

    Error SetAlertProcessors(const AlertRules& alertRules, const ResourceLevel& level,
        const Optional<InstanceIdent>& instanceIdent, Array<AlertProcessor>& processors);

    Average                                mAverage;
    Config                                 mConfig;
    nodeconfig::NodeConfigProviderItf*     mNodeConfigProvider {};
    NodeInfo                               mNodeInfo;
    iamclient::CurrentNodeInfoProviderItf* mCurrentNodeInfoProvider {};
    SenderItf*                             mSender {};
    alerts::SenderItf*                     mAlertSender {};
    NodeMonitoringProviderItf*             mNodeMonitoringProvider {};
    InstanceInfoProviderItf*               mInstanceInfoProvider {};

    StaticAllocator<cAllocatorSize>         mAllocator;
    Mutex                                   mMutex;
    bool                                    mIsRunning {};
    NodeMonitoringData                      mMonitoringData;
    Timer                                   mTimer;
    AlertProcessorArray                     mNodeAlertProcessors;
    StaticArray<Instance, cMaxNumInstances> mWatchedInstances;
};

} // namespace aos::monitoring

#endif
