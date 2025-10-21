/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_LAUNCHER_HPP_
#define AOS_CORE_CM_LAUNCHER_LAUNCHER_HPP_

#include <core/cm/storagestate/itf/storagestate.hpp>
#include <core/cm/unitconfig/itf/nodeconfigprovider.hpp>

#include "itf/imageinfoprovider.hpp"
#include "itf/instancestatusreceiver.hpp"
#include "itf/launcher.hpp"
#include "itf/storage.hpp"

#include "balancer.hpp"
#include "instancemanager.hpp"
#include "nodemanager.hpp"

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Launcher class manages lifecycle of service instances.
 */
class Launcher : public LauncherItf, public InstanceStatusProviderItf, private InstanceStatusReceiverItf {
public:
    /**
     * Initializes launcher object instance.
     *
     * @param config configuration.
     * @param storage storage interface.
     * @param nodeInfoProvider interface providing information about all unit nodes.
     * @param runner instance runner interface.
     * @param imageInfoProvider interface that retrieves service information from its image.
     * @param nodeConfigProvider node config provider interface.
     * @param storageState interface to manage storage and state partitions.
     * @param networkManager interface to manage networks of service instances.
     * @return Error.
     */
    Error Init(const Config& config, StorageItf& storage, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
        InstanceRunnerItf& runner, ImageInfoProviderItf& imageInfoProvider,
        unitconfig::NodeConfigProviderItf& nodeConfigProvider, storagestate::StorageStateItf& storageState,
        networkmanager::NetworkManagerItf& networkManager, MonitoringProviderItf& monitorProvider);

    /**
     * Starts launcher instance.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops launcher instance.
     */
    Error Stop();

    /************************************************************************************************************************
     * LauncherItf implementation
     ***********************************************************************************************************************/

    /**
     * Schedules and run instances.
     *
     * @param instances instances info.
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    Error RunInstances(const Array<RunInstanceRequest>& instances) override;

    /**
     * Rebalances instances.
     *
     * @return Error.
     */
    Error Rebalance() override;

    /************************************************************************************************************************
     * InstanceStatusProviderItf implementation
     ***********************************************************************************************************************/

    /**
     * Returns current statuses of running instances.
     *
     * @param statuses instances statuses.
     * @return Error.
     */
    Error GetInstancesStatuses(Array<InstanceStatus>& statuses) override;

    /**
     * Subscribes status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    Error SubscribeListener(InstanceStatusListenerItf& listener) override;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    Error UnsubscribeListener(InstanceStatusListenerItf& listener) override;

private:
    static constexpr auto cMaxNumInstanceStatusListeners = 1;

    void SendRunStatus();

    void NotifyInstanceStatusListeners();
    void UpdateData(bool rebalancing);

    /************************************************************************************************************************
     * smcontroller::InstanceStatusReceiverItf implementation
     ***********************************************************************************************************************/

    Error OnInstanceStatusReceived(const InstanceStatus& status) override;
    Error OnNodeInstancesStatusesReceived(const String& nodeID, const Array<InstanceStatus>& statuses) override;

    Config                                 mConfig;
    StorageItf*                            mStorage            = nullptr;
    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider   = nullptr;
    InstanceRunnerItf*                     mRunner             = nullptr;
    ImageInfoProviderItf*                  mImageInfoProvider  = nullptr;
    unitconfig::NodeConfigProviderItf*     mNodeConfigProvider = nullptr;
    storagestate::StorageStateItf*         mStorageState       = nullptr;
    networkmanager::NetworkManagerItf*     mNetworkManager     = nullptr;
    MonitoringProviderItf*                 mMonitorProvider    = nullptr;

    InstanceManager mInstanceManager;
    NodeManager     mNodeManager;

    StaticArray<RunInstanceRequest, cMaxNumInstances>                       mRunRequests;
    StaticArray<InstanceStatusListenerItf*, cMaxNumInstanceStatusListeners> mInstanceStatusListeners;
    Balancer                                                                mBalancer;

    Mutex mMutex;
    StaticAllocator<sizeof(StaticArray<InstanceStatus, cMaxNumInstances>) + sizeof(monitoring::NodeMonitoringData)>
        mAllocator;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
