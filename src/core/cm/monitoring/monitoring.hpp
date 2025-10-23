/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_MONITORING_MONITORING_HPP_
#define AOS_CM_MONITORING_MONITORING_HPP_

#include <core/common/cloudconnection/itf/cloudconnection.hpp>
#include <core/common/monitoring/monitoring.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/timer.hpp>

#include <core/cm/launcher/itf/instancestatusprovider.hpp>
#include <core/cm/nodeinfoprovider/itf/nodeinfoprovider.hpp>

#include "config.hpp"
#include "itf/receiver.hpp"
#include "itf/sender.hpp"

namespace aos::cm::monitoring {

/**
 * Monitoring.
 */
class Monitoring : public ReceiverItf,
                   private nodeinfoprovider::NodeInfoListenerItf,
                   private launcher::InstanceStatusListenerItf,
                   private ConnectionListenerItf {
public:
    /**
     * Initializes monitoring.
     *
     * @param config monitoring configuration.
     * @param sender monitoring sender object.
     * @param instanceStatusProvider instance status provider.
     * @param nodeInfoProvider node info provider.
     * @return Error.
     */
    Error Init(const Config& config, SenderItf& sender, launcher::InstanceStatusProviderItf& instanceStatusProvider,
        nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider);

    /**
     * Starts monitoring module.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops monitoring module.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Receives monitoring data.
     *
     * @param monitoring monitoring data.
     * @return Error.
     */
    Error OnMonitoringReceived(const aos::monitoring::NodeMonitoringData& monitoring) override;

    /**
     * Notifies about node info changed.
     *
     * @param info node information.
     */
    void OnNodeInfoChanged(const UnitNodeInfo& info) override;

    /**
     * Notifies about instances statuses change.
     *
     * @param statuses instances statuses.
     */
    void OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses) override;

    /**
     * Notifies publisher is connected.
     */
    void OnConnect() override;

    /**
     * Notifies publisher is disconnected.
     */
    void OnDisconnect() override;

private:
    Error FillNodeMonitoring(const String& nodeID, const aos::monitoring::NodeMonitoringData& nodeMonitoring);
    Error FillInstanceMonitoring(const aos::monitoring::InstanceMonitoringData& instanceMonitoring);
    Error CacheMonitoringData(const aos::monitoring::NodeMonitoringData& monitoringData);
    Error SendMonitoringData();

    Config                                 mConfig;
    SenderItf*                             mSender {};
    launcher::InstanceStatusProviderItf*   mInstanceStatusProvider {};
    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider {};
    Mutex                                  mMutex;
    bool                                   mIsRunning {};
    bool                                   mIsConnected {};
    aos::Monitoring                        mMonitoring;
    Timer                                  mSendTimer;
};

} // namespace aos::cm::monitoring

#endif
