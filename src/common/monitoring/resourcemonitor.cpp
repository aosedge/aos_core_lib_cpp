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

// cppcheck-suppress constParameter
Error ResourceMonitor::Init(iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    ResourceUsageProviderItf& resourceUsageProvider, SenderItf& monitorSender,
    ConnectionPublisherItf& connectionPublisher)
{
    LOG_DBG() << "Init resource monitor";

    mResourceUsageProvider = &resourceUsageProvider;
    mMonitorSender         = &monitorSender;
    mConnectionPublisher   = &connectionPublisher;

    auto nodeInfo = MakeUnique<NodeInfo>(&mAllocator);

    if (auto err = nodeInfoProvider.GetNodeInfo(*nodeInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mNodeMonitoringData.mNodeID         = nodeInfo->mNodeID;
    mNodeMonitoringData.mMonitoringData = MonitoringData {0, 0, nodeInfo->mPartitions, 0, 0};
    mMaxDMIPS                           = nodeInfo->mMaxDMIPS;

    if (auto err = mAverage.Init(nodeInfo->mPartitions, cAverageWindow / cPollPeriod); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mConnectionPublisher->Subscribe(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::Start()
{
    LOG_DBG() << "Start monitoring";

    if (auto err = mThread.Run([this](void*) { ProcessMonitoring(); }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::Stop()
{
    LOG_DBG() << "Stop monitoring";

    mConnectionPublisher->Unsubscribe(*this);

    {
        LockGuard lock {mMutex};

        mFinishMonitoring = true;
        mCondVar.NotifyOne();
    }

    mThread.Join();

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

    if (mInstanceMonitoringData.At(instanceID).mError.IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eAlreadyExist, "instance monitoring already started"));
    }

    if (auto err = mInstanceMonitoringData.Emplace(
            instanceID, InstanceMonitoringData {monitoringConfig.mInstanceIdent, monitoringConfig});
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mAverage.StartInstanceMonitoring(monitoringConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::StopInstanceMonitoring(const String& instanceID)
{
    LockGuard lock {mMutex};

    Error stopErr;

    LOG_DBG() << "Stop instance monitoring: instanceID=" << instanceID;

    auto instanceData = mInstanceMonitoringData.At(instanceID);
    if (!instanceData.mError.IsNone() && stopErr.IsNone()) {
        if (instanceData.mError.Is(ErrorEnum::eNotFound)) {
            LOG_WRN() << "Instance monitoring not found: instanceID=" << instanceID;

            return ErrorEnum::eNone;
        }

        stopErr = AOS_ERROR_WRAP(instanceData.mError);
    }

    if (auto err = mInstanceMonitoringData.Remove(instanceID); !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    if (auto err = mAverage.StopInstanceMonitoring(instanceData.mValue.mInstanceIdent);
        !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    return stopErr;
}

Error ResourceMonitor::GetAverageMonitoringData(NodeMonitoringData& monitoringData)
{
    LockGuard lock {mMutex};

    auto err = mAverage.GetData(monitoringData);
    if (!err.IsNone()) {
        return err;
    }

    monitoringData.mTimestamp = Time::Now();
    monitoringData.mNodeID    = mNodeMonitoringData.mNodeID;

    return ErrorEnum::eNone;
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

        if (auto err = mResourceUsageProvider->GetNodeMonitoringData(
                mNodeMonitoringData.mNodeID, mNodeMonitoringData.mMonitoringData);
            !err.IsNone()) {
            LOG_ERR() << "Failed to get node monitoring data: " << err;
        }

        mNodeMonitoringData.mMonitoringData.mCPU = mNodeMonitoringData.mMonitoringData.mCPU * mMaxDMIPS / 100.0;

        mNodeMonitoringData.mServiceInstances.Clear();

        for (auto& [instanceID, instanceMonitoringData] : mInstanceMonitoringData) {
            if (auto err = mResourceUsageProvider->GetInstanceMonitoringData(instanceID, instanceMonitoringData);
                !err.IsNone()) {
                LOG_ERR() << "Failed to get instance monitoring data: " << err;
            }

            instanceMonitoringData.mMonitoringData.mCPU
                = instanceMonitoringData.mMonitoringData.mCPU * mMaxDMIPS / 100.0;

            mNodeMonitoringData.mServiceInstances.PushBack(instanceMonitoringData);
        }

        if (auto err = mAverage.Update(mNodeMonitoringData); !err.IsNone()) {
            LOG_ERR() << "Failed to update average monitoring data: err=" << err;
        }

        if (!mSendMonitoring) {
            continue;
        }

        if (auto err = mMonitorSender->SendMonitoringData(mNodeMonitoringData); !err.IsNone()) {
            LOG_ERR() << "Failed to send monitoring data: " << err;
        }
    }
}

} // namespace aos::monitoring
