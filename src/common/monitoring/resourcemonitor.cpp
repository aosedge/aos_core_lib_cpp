/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/monitoring/resourcemonitor.hpp"

#include "log.hpp"

namespace aos::monitoring {

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
        return AOS_ERROR_WRAP(err);
    }

    mNodeMonitoringData.mNodeID         = nodeInfo.mNodeID;
    mNodeMonitoringData.mMonitoringData = MonitoringData {0, 0, nodeInfo.mPartitions, 0, 0};

    if (!(err = mAverage.Init(nodeInfo.mPartitions, cAverageWindow / cPollPeriod)).IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!(err = mConnectionPublisher->Subscribes(*this)).IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!(err = mThread.Run([this](void*) { ProcessMonitoring(); })).IsNone()) {
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

    auto findInstance = mInstanceMonitoringData.At(instanceID);
    if (findInstance.mError.IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eAlreadyExist, "instance monitoring already started"));
    }

    auto err = mInstanceMonitoringData.Emplace(instanceID,
        InstanceMonitoringData {monitoringConfig.mInstanceIdent, {0, 0, monitoringConfig.mPartitions, 0, 0}});
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!(mAverage.StartInstanceMonitoring(monitoringConfig)).IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::StopInstanceMonitoring(const String& instanceID)
{
    LockGuard lock {mMutex};

    Error err;

    LOG_DBG() << "Stop instance monitoring: instanceID=" << instanceID;

    auto instanceData = mInstanceMonitoringData.At(instanceID);
    if (!instanceData.mError.IsNone()) {
        err = AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance monitoring not found"));
    }

    auto removeError = mInstanceMonitoringData.Remove(instanceID);
    if (!removeError.IsNone() && err.IsNone()) {
        err = AOS_ERROR_WRAP(removeError);
    }

    auto averageError = mAverage.StopInstanceMonitoring(instanceData.mValue.mInstanceIdent);
    if (!averageError.IsNone() && err.IsNone()) {
        err = AOS_ERROR_WRAP(averageError);
    }

    return err;
}

Error ResourceMonitor::GetAverageMonitoringData(NodeMonitoringData& monitoringData)
{
    LockGuard lock(mMutex);

    auto err = mAverage.GetData(monitoringData);
    if (!err.IsNone()) {
        return err;
    }

    monitoringData.mTimestamp = Time::Now();
    monitoringData.mNodeID    = mNodeMonitoringData.mNodeID;

    return ErrorEnum::eNone;
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

        mNodeMonitoringData.mTimestamp = Time::Now();

        auto err = mResourceUsageProvider->GetNodeMonitoringData(
            mNodeMonitoringData.mNodeID, mNodeMonitoringData.mMonitoringData);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to get node monitoring data: " << err;
        }

        mNodeMonitoringData.mServiceInstances.Clear();

        for (auto& [instanceID, instanceMonitoringData] : mInstanceMonitoringData) {
            err = mResourceUsageProvider->GetInstanceMonitoringData(instanceID, instanceMonitoringData.mMonitoringData);
            if (!err.IsNone()) {
                LOG_ERR() << "Failed to get instance monitoring data: " << err;
            }

            mNodeMonitoringData.mServiceInstances.PushBack(instanceMonitoringData);
        }

        if (!(err = mAverage.Update(mNodeMonitoringData)).IsNone()) {
            LOG_ERR() << "Failed to update average monitoring data: err=" << err;
        }

        if (!mSendMonitoring) {
            continue;
        }

        if (!(err = mMonitorSender->SendMonitoringData(mNodeMonitoringData)).IsNone()) {
            LOG_ERR() << "Failed to send monitoring data: " << err;
        }
    }
}

} // namespace aos::monitoring
