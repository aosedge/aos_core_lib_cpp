/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_SM_LAUNCHER_LAUNCHER_HPP_
#define AOS_CORE_SM_LAUNCHER_LAUNCHER_HPP_

#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>
#include <core/common/monitoring/itf/instanceinfoprovider.hpp>
#include <core/common/tools/allocator.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/instance.hpp>
#include <core/sm/config.hpp>

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
                 public monitoring::InstanceInfoProviderItf {
public:
    /**
     * Initializes launcher.
     *
     * @param runtimes available runtimes.
     * @param statusSender sender.
     * @param storage storage.
     *
     * @return Error.
     */
    Error Init(const Array<RuntimeItf*>& runtimes, SenderItf& sender, StorageItf& storage);

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

    static constexpr auto cMaxNumSubscribers = 4;
    static constexpr auto cAllocatorSize     = sizeof(StaticArray<InstanceIdent, cMaxNumInstances>)
        + 2 * sizeof(InstanceInfoArray) + sizeof(InstanceStatusArray);

    struct InstanceData {
        InstanceInfo   mInfo;
        InstanceStatus mStatus;
    };

    void  RunRebootThread();
    Error StoreInstalledComponent(const aos::InstanceStatus& status);
    void  UpdateInstancesImpl(const Array<InstanceIdent>& stopInstances, const Array<InstanceInfo>& startInstances);
    void  StopInstances(const Array<InstanceIdent>& stopInstances, Array<InstanceStatus>& statuses);
    void  StartInstances(const Array<InstanceInfo>& startInstances);
    void  StartInstance(RuntimeItf& runtime, InstanceData& instance);
    void  ClearCachedInstances();
    Error StartStoredInstances();
    Error StartLaunch();
    void  FinishLaunch();
    bool  IsPreinstalledInstance(const InstanceStatus& status) const;

    InstanceData* FindInstanceData(const InstanceIdent& instanceIdent);
    InstanceData* FindInstanceData(const InstanceIdent& instanceIdent) const;
    RuntimeItf*   FindInstanceRuntime(const String& runtimeID);
    RuntimeItf*   FindInstanceRuntime(const String& runtimeID) const;
    RuntimeItf*   FindInstanceRuntime(const InstanceIdent& instanceIdent);
    RuntimeItf*   FindInstanceRuntime(const InstanceIdent& instanceIdent) const;

    mutable StaticAllocator<cAllocatorSize>                               mAllocator;
    StaticArray<instancestatusprovider::ListenerItf*, cMaxNumSubscribers> mSubscribers;
    Thread<cThreadTaskSize>                                               mThread;
    Thread<cThreadTaskSize>                                               mRebootThread;
    ThreadPool<cMaxNumConcurrentItems, cMaxNumInstances, cThreadTaskSize> mLaunchPool;
    mutable Mutex                                                         mMutex;
    mutable ConditionalVariable                                           mCondVar;
    StaticArray<InstanceData, cMaxNumInstances>                           mInstances;
    StaticMap<RuntimeItf*, StaticString<cIDLen>, cMaxNumNodeRuntimes>     mRuntimes;
    StaticArray<StaticString<cIDLen>, cMaxNumNodeRuntimes>                mRebootQueue;
    StorageItf*                                                           mStorage {};
    SenderItf*                                                            mSender {};
    bool                                                                  mLaunchInProgress {};
    bool                                                                  mIsRunning {};
    bool                                                                  mFirstStart {true};
};

} // namespace aos::sm::launcher

#endif
