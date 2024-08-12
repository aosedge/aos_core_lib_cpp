/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/monitoring.hpp"

#include "log.hpp"

namespace aos {
namespace monitoring {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ResourceMonitor::Init(iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    ResourceUsageProviderItf& resourceUsageProvider, SenderItf& monitorSender,
    ConnectionPublisherItf& connectionPublisher)
{
    LOG_DBG() << "Init resource monitor";

    mResourceUsageProvider = &resourceUsageProvider;
    mMonitorSender         = &monitorSender;
    mConnectionPublisher   = &connectionPublisher;

    NodeInfo nodeInfo;

    auto err = nodeInfoProvider.GetNodeInfo(nodeInfo);
    if (!err.IsNone()) {
        return err;
    }

    mNodeMonitoringData.mNodeID         = nodeInfo.mNodeID;
    mNodeMonitoringData.mMonitoringData = {};

    for (const auto& disk : nodeInfo.mPartitions) {
        mNodeMonitoringData.mMonitoringData.mDisk.EmplaceBack(disk);
    }

    err = mConnectionPublisher->Subscribes(*this);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mThread.Run([this](void*) { ProcessMonitoring(); });
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void ResourceMonitor::OnConnect()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Connection event";

    mSendMonitoring = true;
}

void ResourceMonitor::OnDisconnect()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Disconnection event";

    mSendMonitoring = false;
}

Error ResourceMonitor::StartInstanceMonitoring(const String& instanceID, const InstanceMonitorParams& monitoringConfig)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start instance monitoring: instanceID=" << instanceID;

    auto findInstance = mNodeMonitoringData.mServiceInstances.Find(
        [&instanceID](const InstanceMonitoringData& instance) { return instance.mInstanceID == instanceID; });

    if (!findInstance.mError.IsNone()) {
        MonitoringData monitoringData {};

        for (const auto& disk : monitoringConfig.mPartitions) {
            monitoringData.mDisk.EmplaceBack(disk);
        }

        mNodeMonitoringData.mServiceInstances.EmplaceBack(instanceID, monitoringConfig.mInstanceIdent, monitoringData);

        return ErrorEnum::eNone;
    }

    findInstance.mValue->mMonitoringData.mDisk.Clear();

    for (const auto& disk : monitoringConfig.mPartitions) {
        findInstance.mValue->mMonitoringData.mDisk.EmplaceBack(disk);
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::StopInstanceMonitoring(const String& instanceID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Stop instance monitoring: instanceID=" << instanceID;

    return mNodeMonitoringData.mServiceInstances
        .Remove([&instanceID](const InstanceMonitoringData& instance) { return instance.mInstanceID == instanceID; })
        .mError;
}

ResourceMonitor::~ResourceMonitor()
{
    mConnectionPublisher->Unsubscribes(*this);

    {
        LockGuard lock {mMutex};

        mFinishMonitoring = true;
        mCondVar.NotifyOne();
    }

    mThread.Join();
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void ResourceMonitor::ProcessMonitoring()
{
    while (true) {
        UniqueLock lock {mMutex};

        mCondVar.Wait(lock, cPollPeriod, [this] { return mFinishMonitoring; });

        if (mFinishMonitoring) {
            break;
        }

        if (!mSendMonitoring) {
            continue;
        }

        clock_gettime(CLOCK_REALTIME, &mNodeMonitoringData.mTimestamp);

        auto err = mResourceUsageProvider->GetNodeMonitoringData(
            mNodeMonitoringData.mNodeID, mNodeMonitoringData.mMonitoringData);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to get node monitoring data: " << err;
        }

        for (auto& instance : mNodeMonitoringData.mServiceInstances) {
            err = mResourceUsageProvider->GetInstanceMonitoringData(instance.mInstanceID, instance.mMonitoringData);
            if (!err.IsNone()) {
                LOG_ERR() << "Failed to get instance monitoring data: " << err;
            }
        }

        LOG_DBG() << "Send monitoring data";

        err = mMonitorSender->SendMonitoringData(mNodeMonitoringData);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to send monitoring data: " << err;
        }
    }
}

} // namespace monitoring
} // namespace aos
