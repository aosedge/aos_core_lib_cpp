/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_LAUNCHER_HPP_
#define AOS_CORE_CM_LAUNCHER_LAUNCHER_HPP_

#include <core/cm/alerts/itf/provider.hpp>
#include <core/cm/imagemanager/itf/blobinfoprovider.hpp>
#include <core/cm/imagemanager/itf/iteminfoprovider.hpp>
#include <core/cm/storagestate/itf/storagestate.hpp>
#include <core/cm/unitconfig/itf/nodeconfigprovider.hpp>
#include <core/common/iamclient/itf/identprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>

#include "itf/envvarhandler.hpp"
#include "itf/instancestatusreceiver.hpp"
#include "itf/launcher.hpp"
#include "itf/storage.hpp"

#include "balancer.hpp"
#include "instancemanager.hpp"
#include "networkmanager.hpp"
#include "nodemanager.hpp"

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Launcher class manages lifecycle of service instances.
 */
class Launcher : public LauncherItf,
                 public InstanceStatusReceiverItf,
                 public EnvVarHandlerItf,
                 private nodeinfoprovider::NodeInfoListenerItf,
                 private alerts::AlertsListenerItf,
                 private iamclient::SubjectsListenerItf {
public:
    /**
     * Initializes launcher object instance.
     *
     * @param config configuration.
     * @param nodeInfoProvider interface providing information about all unit nodes.
     * @param runner instance runner interface.
     * @param imageInfoProvider interface that retrieves service information from its image.
     * @param ociSpec OCI spec interface.
     * @param nodeConfigProvider node config provider interface.
     * @param storageState interface to manage storage and state partitions.
     * @param networkManager interface to manage networks of service instances.
     * @param monitorProvider monitoring provider.
     * @param alertsProvider alerts provider.
     * @param identProvider ident provider.
     * @param gidValidator GID validator.
     * @param uidValidator UID validator.
     * @param storage storage interface.
     * @return Error.
     */
    Error Init(const Config& config, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider, InstanceRunnerItf& runner,
        imagemanager::ItemInfoProviderItf& itemInfoProvider, oci::OCISpecItf& ociSpec,
        unitconfig::NodeConfigProviderItf& nodeConfigProvider, storagestate::StorageStateItf& storageState,
        networkmanager::NetworkManagerItf& networkManager, MonitoringProviderItf& monitorProvider,
        alerts::AlertsProviderItf& alertsProvider, iamclient::IdentProviderItf& identProvider,
        IdentifierPoolValidator gidValidator, IdentifierPoolValidator uidValidator, StorageItf& storage);

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

    //
    // LauncherItf implementation
    //

    /**
     * Schedules and run instances.
     *
     * @param instances instances info.
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    Error RunInstances(const Array<RunInstanceRequest>& instances, Array<InstanceStatus>& statuses) override;

    //
    // InstanceStatusProviderItf implementation
    //

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

    //
    // EnvVarHandlerItf implementation
    //

    /**
     * Overrides environment variables.
     *
     * @param envVars environment variables.
     * @return Error.
     */
    Error OverrideEnvVars(const OverrideEnvVarsRequest& envVars) override;

private:
    static constexpr auto cMaxNumInstanceStatusListeners = 8;
    static constexpr auto cAllocatorSize                 = 2 * sizeof(StaticArray<InstanceStatus, cMaxNumInstances>)
        + sizeof(StaticArray<SharedPtr<Instance>, cMaxNumInstances>);

    void SendRunStatus();

    void UpdateInstanceStatuses();
    void FailActivatingInstances();

    Error Rebalance(UniqueLock<Mutex>& lock);

    void ProcessUpdate();
    void WaitAllNodesConnected(UniqueLock<Mutex>& lock);

    void CreateRequestedInstances(Array<SharedPtr<Instance>>& instances);
    void CreateRequestedInstances(const Array<RunInstanceRequest>& requests, Array<SharedPtr<Instance>>& instances);

    // InstanceStatusReceiverItf implementation
    Error OnInstanceStatusReceived(const InstanceStatus& status) override;
    Error OnNodeInstancesStatusesReceived(const String& nodeID, const Array<InstanceStatus>& statuses) override;

    // nodeinfoprovider::NodeInfoListenerItf implementation
    void OnNodeInfoChanged(const UnitNodeInfo& info) override;

    // alerts::AlertsListenerItf implementation
    Error OnAlertReceived(const AlertVariant& alert) override;

    // iamclient::SubjectsListenerItf implementation
    void SubjectsChanged(const Array<StaticString<cIDLen>>& subjects) override;

    // External dependencies
    Config                                                                            mConfig;
    StorageItf*                                                                       mStorage {};
    nodeinfoprovider::NodeInfoProviderItf*                                            mNodeInfoProvider {};
    iamclient::IdentProviderItf*                                                      mIdentProvider {};
    InstanceRunnerItf*                                                                mRunner {};
    unitconfig::NodeConfigProviderItf*                                                mNodeConfigProvider {};
    storagestate::StorageStateItf*                                                    mStorageState {};
    MonitoringProviderItf*                                                            mMonitorProvider {};
    alerts::AlertsProviderItf*                                                        mAlertsProvider {};
    StaticArray<instancestatusprovider::ListenerItf*, cMaxNumInstanceStatusListeners> mInstanceStatusListeners;

    // Managers
    NetworkManager  mNetworkManager {};
    InstanceManager mInstanceManager {};
    NodeManager     mNodeManager {};
    Balancer        mBalancer {};

    // Process update thread
    Thread<>                                        mWorkerThread;
    Mutex                                           mUpdateMutex;
    ConditionalVariable                             mProcessUpdatesCondVar;
    bool                                            mDisableProcessUpdates {};
    StaticArray<StaticString<cIDLen>, cMaxNumNodes> mUpdatedNodes;
    bool                                            mAlertReceived {};
    bool                                            mIsNodeInfoChanged {};
    Optional<SubjectArray>                          mNewSubjects;

    // Misc
    StaticArray<InstanceStatus, cMaxNumInstances> mInstanceStatuses;
    Mutex                                         mBalancingMutex;
    ConditionalVariable                           mAllNodesConnectedCondVar;
    bool                                          mIsRunning {};
    StaticAllocator<cAllocatorSize>               mAllocator;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
