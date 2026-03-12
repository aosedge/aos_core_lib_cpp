/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_SM_LAUNCHER_LAUNCHER_HPP_
#define AOS_CORE_SM_LAUNCHER_LAUNCHER_HPP_

#include <core/common/cloudconnection/itf/cloudconnection.hpp>
#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>
#include <core/common/monitoring/itf/instanceinfoprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/allocator.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/instance.hpp>
#include <core/sm/imagemanager/imagemanager.hpp>

#include "itf/instancestatusreceiver.hpp"
#include "itf/launcher.hpp"
#include "itf/runtime.hpp"
#include "itf/runtimeinfoprovider.hpp"
#include "itf/sender.hpp"
#include "itf/storage.hpp"

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Launcher implementation.
 */
class Launcher : public LauncherItf,
                 public InstanceStatusReceiverItf,
                 public RuntimeInfoProviderItf,
                 public monitoring::InstanceInfoProviderItf,
                 private cloudconnection::ConnectionListenerItf {
public:
    /**
     * Initializes launcher.
     *
     * @param runtimes available runtimes.
     * @param imageManager image manager.
     * @param statusSender sender.
     * @param storage storage.
     * @param ociSpec OCI spec.
     * @param itemInfoProvider item info provider.
     * @param cloudConnection cloud connection.
     *
     * @return Error.
     */
    Error Init(const Array<RuntimeItf*>& runtimes, imagemanager::ImageManagerItf& imageManager, SenderItf& sender,
        StorageItf& storage, oci::OCISpecItf& ociSpec, imagemanager::ItemInfoProviderItf& itemInfoProvider,
        cloudconnection::CloudConnectionItf& cloudConnection);

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
     * Runs instances.
     *
     * @param instances instances to run.
     * @return Error.
     */
    Error RunInstances(const Array<InstanceInfo>& instances) override;

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
    struct InstanceData {
        InstanceInfo   mInfo;
        InstanceStatus mStatus;
        Duration       mOfflineTTL;
    };

    struct UpdateItemInfo {
        StaticString<cIDLen>      mItemID;
        StaticString<cVersionLen> mVersion;
    };

    static constexpr auto cThreadTaskSize    = 512;
    static constexpr auto cMaxNumSubscribers = 4;
    static constexpr auto cAllocatorSize     = 2 * sizeof(StaticArray<InstanceIdent, cMaxNumInstances>)
        + 2 * sizeof(InstanceInfoArray) + sizeof(InstanceStatusArray) + sizeof(oci::ImageManifest)
        + sizeof(oci::ItemConfig) + sizeof(StaticString<cFilePathLen>)
        + Max(sizeof(StaticArray<UpdateItemInfo, cMaxNumUpdateItems>),
            sizeof(StaticArray<imagemanager::UpdateItemInfo, cMaxNumUpdateItems>)
                + sizeof(StaticArray<imagemanager::UpdateItemStatus, cMaxNumUpdateItems>));

    void  OnConnect() override;
    void  OnDisconnect() override;
    void  RunRebootThread();
    void  HandleOfflineTTLs();
    Error HandleComponentStatus(const aos::InstanceStatus& status);
    void  SortStartInstances();
    void  RunInstancesImpl();
    Error SetStopInstances();
    void  StopInstances(const Array<InstanceIdent>& stopInstances);
    Error StopInstance(InstanceData& instanceData);
    void  StopAllInstances();
    void  StartInstances(const Array<InstanceInfo>& startInstances);
    Error StartInstance(InstanceData& instanceData);
    Error AppendInstancesWithModifiedParams(
        const Array<InstanceInfo>& startInstances, Array<InstanceIdent>& stopInstances);
    Error StartLaunch();
    void  FinishLaunch();
    void  GetRemoveUpdateItems(const Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances,
         Array<UpdateItemInfo>& removeItems);
    void  RemoveUpdateItems(const Array<UpdateItemInfo>& removeItems);
    void  InstallUpdateItems(const Array<InstanceInfo>& startInstances);
    RetWithError<InstanceData*> AddInstanceData(const InstanceInfo& instanceInfo);
    Error                       RemoveInstanceData(const InstanceIdent& instanceIdent);
    void                        RemoveInstancesData(const Array<InstanceIdent>& instances);
    void SetInstanceState(InstanceData& instance, const InstanceState& state, const Error& error = ErrorEnum::eNone);

    InstanceData*          FindInstanceData(const InstanceIdent& instanceIdent);
    InstanceData*          FindInstanceData(const InstanceIdent& instanceIdent) const;
    RuntimeItf*            FindInstanceRuntime(const String& runtimeID);
    RuntimeItf*            FindInstanceRuntime(const String& runtimeID) const;
    RuntimeItf*            FindInstanceRuntime(const InstanceIdent& instanceIdent);
    RuntimeItf*            FindInstanceRuntime(const InstanceIdent& instanceIdent) const;
    RetWithError<Duration> GetOfflineTTL(const InstanceInfo& instanceInfo);
    Optional<Duration>     GetMinOfflineTTL() const;
    void                   StartTTLTimer();
    void                   StopExpiredInstances(UniqueLock<Mutex>& lock);
    void                   SendNodeInstancesStatuses();

    mutable StaticAllocator<cAllocatorSize>                               mAllocator;
    StaticArray<instancestatusprovider::ListenerItf*, cMaxNumSubscribers> mSubscribers;
    Thread<cThreadTaskSize>                                               mThread;
    Thread<cThreadTaskSize>                                               mRebootThread;
    Timer                                                                 mOfflineTTLHandler;
    ThreadPool<cMaxNumConcurrentItems, cMaxNumInstances, cThreadTaskSize> mLaunchPool;
    ThreadPool<cMaxNumConcurrentItems, cMaxNumInstances, cThreadTaskSize> mOfflineTTLPool;
    mutable Mutex                                                         mMutex;
    Mutex                                                                 mSubscribersMutex;
    mutable ConditionalVariable                                           mCondVar;
    StaticArray<InstanceData, cMaxNumInstances>                           mInstances;
    StaticMap<RuntimeItf*, StaticString<cIDLen>, cMaxNumNodeRuntimes>     mRuntimes;
    StaticArray<StaticString<cIDLen>, cMaxNumNodeRuntimes>                mRebootQueue;
    imagemanager::ImageManagerItf*                                        mImageManager {};
    StorageItf*                                                           mStorage {};
    SenderItf*                                                            mSender {};
    oci::OCISpecItf*                                                      mOCISpec {};
    imagemanager::ItemInfoProviderItf*                                    mItemInfoProvider {};
    cloudconnection::CloudConnectionItf*                                  mCloudConnection {};
    bool                                                                  mLaunchInProgress {};
    bool                                                                  mIsRunning {};
    bool                                                                  mFirstStart {true};
    Optional<Time>                                                        mOfflineTime {};
    StaticArray<InstanceIdent, cMaxNumInstances>                          mStopInstances;
    InstanceInfoArray                                                     mStartInstances;
};

} // namespace aos::sm::launcher

#endif
