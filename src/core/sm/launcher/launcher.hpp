/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_SM_LAUNCHER_LAUNCHER_HPP_
#define AOS_CORE_SM_LAUNCHER_LAUNCHER_HPP_

#include <core/common/cloudconnection/itf/cloudconnection.hpp>
#include <core/common/imagemanager/itf/iteminfoprovider.hpp>
#include <core/common/imagemanager/itf/itemstatusprovider.hpp>
#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>
#include <core/common/monitoring/itf/instanceinfoprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/instance.hpp>
#include <core/sm/imagemanager/itf/imagemanager.hpp>
#include <core/sm/networkmanager/itf/networkmanager.hpp>
#include <core/sm/resourcemanager/itf/resourceinfoprovider.hpp>

#include "instanceregistry.hpp"
#include "instancerunner.hpp"
#include "itf/instanceidprovider.hpp"
#include "itf/instancestatusreceiver.hpp"
#include "itf/launcher.hpp"
#include "itf/runtime.hpp"
#include "itf/runtimeinfoprovider.hpp"
#include "itf/sender.hpp"
#include "itf/storage.hpp"
#include "offlinehandler.hpp"
#include "runtimemanager.hpp"
#include "statusnotifier.hpp"
#include "updateiteminstaller.hpp"

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Launcher: runs the node instances requested by the CM.
 *
 * The launcher runs one job at a time on its thread: an update request, the repair of the instances reported broken,
 * the stop of the instances whose offline TTL expired and the runtimes reboot. The jobs never overlap, so a reboot or
 * an offline TTL never interrupts a launch.
 */
class Launcher : public LauncherItf,
                 public InstanceStatusReceiverItf,
                 public RuntimeInfoProviderItf,
                 public monitoring::InstanceInfoProviderItf,
                 private OfflineCheckListenerItf {
public:
    /**
     * Initializes launcher.
     *
     * @param allocator allocator to use for temporary objects.
     * @param runtimes available runtimes.
     * @param imageManager image manager.
     * @param itemStatusProvider item status provider.
     * @param statusSender sender.
     * @param storage storage.
     * @param ociSpec OCI spec.
     * @param itemInfoProvider item info provider.
     * @param cloudConnection cloud connection.
     * @param networkManager network manager.
     * @param instanceIDProvider instance ID provider.
     * @param resourceInfoProvider resource info provider.
     *
     * @return Error.
     */
    Error Init(AllocatorItf& allocator, const Array<RuntimeItf*>& runtimes, imagemanager::ImageManagerItf& imageManager,
        common::imagemanager::ItemStatusProviderItf& itemStatusProvider, SenderItf& sender, StorageItf& storage,
        oci::OCISpecItf& ociSpec, common::imagemanager::ItemInfoProviderItf& itemInfoProvider,
        cloudconnection::CloudConnectionItf& cloudConnection, networkmanager::NetworkManagerItf& networkManager,
        InstanceIDProviderItf& instanceIDProvider, resourcemanager::ResourceInfoProviderItf& resourceInfoProvider);

    /**
     * Starts launcher.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops launcher.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Update running instances.
     *
     * @param stopInstances instances to stop.
     * @param startInstances instances to start.
     * @return Error.
     */
    Error UpdateInstances(
        const Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances) override;

    /**
     * Receives instances statuses.
     *
     * @param statuses instances statuses.
     * @return Error.
     */
    Error OnInstancesStatusesReceived(const Array<InstanceStatus>& statuses) override;

    /**
     * Notifies that runtime requires reboot.
     *
     * @param runtimeID runtime identifier.
     * @return Error.
     */
    Error RebootRequired(const String& runtimeID) override;

    /**
     * Returns current statuses of running instances.
     *
     * @param[out] statuses instances statuses.
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

    /**
     * Gets instance monitoring parameters.
     *
     * @param instanceIdent instance ident.
     * @param[out] params instance monitoring parameters.
     * @return Error eNotSupported if instance monitoring is not supported.
     */
    Error GetInstanceMonitoringParams(
        const InstanceIdent& instanceIdent, InstanceMonitoringParams& params) const override;

    /**
     * Returns instance monitoring data.
     *
     * @param instanceIdent instance ident.
     * @param[out] monitoringData instance monitoring data.
     * @return Error.
     */
    Error GetInstanceMonitoringData(
        const InstanceIdent& instanceIdent, monitoring::InstanceMonitoringData& monitoringData) override;

    /**
     * Returns runtimes infos.
     *
     * @param[out] runtimes runtime infos.
     * @return Error.
     */
    Error GetRuntimesInfos(Array<RuntimeInfo>& runtimes) const override;

private:
    static constexpr auto cThreadTaskSize = 512;

    struct UpdateRequest {
        StaticArray<InstanceIdent, cMaxNumInstances> mStopInstances;
        InstanceInfoArray                            mStartInstances;
    };

    void OnOfflineCheck() override;

    void RunJobs();
    void RunUpdate(UniquePtr<UpdateRequest>& request);
    void LaunchInstances(
        Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances, bool moduleStart);
    void AddStartInstances(
        const Array<InstanceInfo>& startInstances, const Array<InstallItem>& installItems, const Error& installError);
    bool RepairBrokenInstances();
    void StopExpiredInstances();
    void ArmOfflineHandler();
    void LoadStoredInstances(const Array<InstanceInfo>& storedInstances);
    void StopAllInstances();
    void RemoveInstances(const Array<InstanceData*>& instances);
    template <typename T>
    void  FindInstances(const Array<T>& idents, Array<InstanceData*>& instances);
    Error AppendInstancesWithModifiedParams(
        const Array<InstanceInfo>& startInstances, Array<InstanceIdent>& stopInstances);
    Error HandleComponentStatus(const aos::InstanceStatus& status);
    void  HandleBrokenInstance(const InstanceStatus& status);
    void  NotifyAll();

    AllocatorItf*                                          mAllocator {};
    StorageItf*                                            mStorage {};
    cloudconnection::CloudConnectionItf*                   mCloudConnection {};
    Thread<cThreadTaskSize>                                mThread;
    mutable Mutex                                          mMutex;
    mutable ConditionalVariable                            mCondVar;
    InstanceRegistry                                       mInstances;
    RuntimeManager                                         mRuntimes;
    InstanceRunner                                         mRunner;
    UpdateItemInstaller                                    mInstaller;
    OfflineHandler                                         mOfflineHandler;
    StatusNotifier                                         mStatusNotifier;
    UniquePtr<UpdateRequest>                               mPendingUpdate;
    StaticArray<StaticString<cIDLen>, cMaxNumNodeRuntimes> mRebootQueue;
    bool                                                   mIsRunning {};
    bool                                                   mFirstStart {true};
    bool                                                   mUpdateInProgress {};
    bool                                                   mRepairRequested {};
    bool                                                   mOfflineCheckRequested {};
};

} // namespace aos::sm::launcher

#endif
