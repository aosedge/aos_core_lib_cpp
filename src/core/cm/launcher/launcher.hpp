/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_LAUNCHER_HPP_
#define AOS_CORE_CM_LAUNCHER_LAUNCHER_HPP_

#include <core/cm/imagemanager/itf/blobinfoprovider.hpp>
#include <core/cm/imagemanager/itf/iteminfoprovider.hpp>
#include <core/cm/storagestate/itf/storagestate.hpp>
#include <core/cm/unitconfig/itf/nodeconfigprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>

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
class Launcher : public LauncherItf,
                 public instancestatusprovider::ProviderItf,
                 public InstanceStatusReceiverItf,
                 private nodeinfoprovider::NodeInfoListenerItf {
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
     * @param monitorProvider monitoring provider.
     * @param gidValidator GID validator.
     * @param uidValidator UID validator.
     * @return Error.
     */
    Error Init(const Config& config, StorageItf& storage, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
        InstanceRunnerItf& runner, imagemanager::ItemInfoProviderItf& itemInfoProvider,
        imagemanager::BlobInfoProviderItf& blobInfoProvider, oci::OCISpecItf& ociSpec,
        unitconfig::NodeConfigProviderItf& nodeConfigProvider, storagestate::StorageStateItf& storageState,
        networkmanager::NetworkManagerItf& networkManager, MonitoringProviderItf& monitorProvider,
        IDValidator gidValidator, IDValidator uidValidator);

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
    Error RunInstances(const Array<RunInstanceRequest>& instances, Array<InstanceStatus>& statuses) override;

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
    Error SubscribeListener(instancestatusprovider::ListenerItf& listener) override;

    /**
     * Unsubscribes from status notifications.
     *
     * @param listener status listener.
     * @return Error.
     */
    Error UnsubscribeListener(instancestatusprovider::ListenerItf& listener) override;

private:
    static constexpr auto cMaxNumInstanceStatusListeners = 1;
    static constexpr auto cAllocatorSize
        = sizeof(StaticArray<InstanceStatus, cMaxNumInstances>) + sizeof(monitoring::NodeMonitoringData);

    void SendRunStatus();

    void NotifyInstanceStatusListeners(Array<InstanceStatus>& statuses);
    void UpdateData(bool rebalancing);
    void FailActivatingInstances();

    /************************************************************************************************************************
     * smcontroller::InstanceStatusReceiverItf implementation
     ***********************************************************************************************************************/

    Error OnInstanceStatusReceived(const InstanceStatus& status) override;
    Error OnNodeInstancesStatusesReceived(const String& nodeID, const Array<InstanceStatus>& statuses) override;
    Error OnEnvVarsStatusesReceived(const String& nodeID, const Array<EnvVarsInstanceStatus>& statuses) override;

    /************************************************************************************************************************
     * nodeinfoprovider::NodeInfoListenerItf implementation
     ***********************************************************************************************************************/

    void OnNodeInfoChanged(const UnitNodeInfo& info) override;

    Config                                 mConfig;
    StorageItf*                            mStorage {};
    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider {};
    InstanceRunnerItf*                     mRunner {};
    unitconfig::NodeConfigProviderItf*     mNodeConfigProvider {};
    storagestate::StorageStateItf*         mStorageState {};
    networkmanager::NetworkManagerItf*     mNetworkManager {};
    MonitoringProviderItf*                 mMonitorProvider {};

    InstanceManager mInstanceManager;
    NodeManager     mNodeManager;

    StaticArray<RunInstanceRequest, cMaxNumInstances>                                 mRunRequests;
    StaticArray<instancestatusprovider::ListenerItf*, cMaxNumInstanceStatusListeners> mInstanceStatusListeners;
    Balancer                                                                          mBalancer;

    StaticArray<InstanceStatus, cMaxNumInstances> mInstanceStatuses;

    Mutex                           mMutex;
    StaticAllocator<cAllocatorSize> mAllocator;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
