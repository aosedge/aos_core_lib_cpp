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
Error ResourceMonitor::Init(const Config& config, iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    sm::resourcemanager::ResourceManagerItf& resourceManager, ResourceUsageProviderItf& resourceUsageProvider,
    SenderItf& monitorSender, alerts::SenderItf& alertSender, ConnectionPublisherItf& connectionPublisher)
{
    LOG_DBG() << "Init resource monitor";

    mConfig                = config;
    mResourceUsageProvider = &resourceUsageProvider;
    mResourceManager       = &resourceManager;
    mMonitorSender         = &monitorSender;
    mAlertSender           = &alertSender;
    mConnectionPublisher   = &connectionPublisher;

    auto nodeInfo = MakeUnique<NodeInfo>(&mAllocator);

    if (auto err = nodeInfoProvider.GetNodeInfo(*nodeInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mNodeMonitoringData.mNodeID                     = nodeInfo->mNodeID;
    mNodeMonitoringData.mMonitoringData.mPartitions = nodeInfo->mPartitions;
    mMaxDMIPS                                       = nodeInfo->mMaxDMIPS;
    mMaxMemory                                      = nodeInfo->mTotalRAM;

    if (auto err = mConnectionPublisher->Subscribe(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    assert(mConfig.mPollPeriod > 0);

    if (auto err = mAverage.Init(nodeInfo->mPartitions, mConfig.mAverageWindow / mConfig.mPollPeriod); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::Start()
{
    LOG_DBG() << "Start monitoring";

    auto nodeConfig = MakeUnique<sm::resourcemanager::NodeConfig>(&mAllocator);

    if (auto err = mResourceManager->GetNodeConfig(nodeConfig->mNodeConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetupSystemAlerts(nodeConfig->mNodeConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

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

Error ResourceMonitor::ReceiveNodeConfig(const sm::resourcemanager::NodeConfig& nodeConfig)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Receive node config: version=" << nodeConfig.mVersion;

    if (auto err = SetupSystemAlerts(nodeConfig.mNodeConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::StartInstanceMonitoring(const String& instanceID, const InstanceMonitorParams& monitoringConfig)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start instance monitoring: instanceID=" << instanceID;

    if (mInstanceMonitoringData.Find(instanceID) != mInstanceMonitoringData.end()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eAlreadyExist, "instance monitoring already started"));
    }

    auto instanceMonitoringData = MakeUnique<InstanceMonitoringData>(&mAllocator, monitoringConfig);

    auto err = mInstanceMonitoringData.Set(instanceID, *instanceMonitoringData);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanUp = DeferRelease(&err, [this, &instanceID](Error* err) {
        if (!err->IsNone()) {
            mInstanceMonitoringData.Remove(instanceID);
            mInstanceAlertProcessors.Remove(instanceID);
        }
    });

    if (err = mResourceUsageProvider->GetInstanceMonitoringData(
            instanceID, mInstanceMonitoringData.Find(instanceID)->mSecond);
        !err.IsNone()) {
        LOG_WRN() << "Can't get instance monitoring data: instanceID=" << instanceID << ", err=" << err;
    }

    if (monitoringConfig.mAlertRules.HasValue() && mAlertSender) {
        if (auto alertsErr = SetupInstanceAlerts(instanceID, monitoringConfig); !alertsErr.IsNone()) {
            LOG_ERR() << "Can't setup instance alerts: instanceID=" << instanceID << ", err=" << alertsErr;
        }
    }

    if (err = mAverage.StartInstanceMonitoring(monitoringConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::StopInstanceMonitoring(const String& instanceID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Stop instance monitoring: instanceID=" << instanceID;

    auto instanceData = mInstanceMonitoringData.Find(instanceID);
    if (instanceData == mInstanceMonitoringData.end()) {
        LOG_WRN() << "Instance monitoring not found: instanceID=" << instanceID;

        return ErrorEnum::eNone;
    }

    Error stopErr;

    if (auto err = mInstanceMonitoringData.Remove(instanceID); !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    if (auto err = mAverage.StopInstanceMonitoring(instanceData->mSecond.mInstanceIdent);
        !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    mInstanceAlertProcessors.Remove(instanceID);

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

String ResourceMonitor::GetParameterName(const ResourceIdentifier& id) const
{
    if (id.mPartitionName.HasValue()) {
        return id.mPartitionName.GetValue();
    }

    return id.mType.ToString();
}

cloudprotocol::AlertVariant ResourceMonitor::CreateSystemQuotaAlertTemplate(
    const ResourceIdentifier& resourceIdentifier) const
{
    cloudprotocol::AlertVariant     alertItem;
    cloudprotocol::SystemQuotaAlert quotaAlert = {};

    quotaAlert.mNodeID    = mNodeMonitoringData.mNodeID;
    quotaAlert.mParameter = GetParameterName(resourceIdentifier);

    alertItem.SetValue<cloudprotocol::SystemQuotaAlert>(quotaAlert);

    return alertItem;
}

cloudprotocol::AlertVariant ResourceMonitor::CreateInstanceQuotaAlertTemplate(
    const InstanceIdent& instanceIdent, const ResourceIdentifier& resourceIdentifier) const
{
    cloudprotocol::AlertVariant       alertItem;
    cloudprotocol::InstanceQuotaAlert quotaAlert = {};

    quotaAlert.mInstanceIdent = instanceIdent;
    quotaAlert.mParameter     = GetParameterName(resourceIdentifier);

    alertItem.SetValue<cloudprotocol::InstanceQuotaAlert>(quotaAlert);

    return alertItem;
}

double ResourceMonitor::CPUToDMIPs(double cpuPersentage) const
{
    return cpuPersentage * static_cast<double>(mMaxDMIPS) / 100.0;
}

Error ResourceMonitor::SetupSystemAlerts(const NodeConfig& nodeConfig)
{
    LOG_DBG() << "Setup system alerts";

    mAlertProcessors.Clear();

    if (!nodeConfig.mAlertRules.HasValue()) {
        return ErrorEnum::eNone;
    }

    const auto& alertRules = nodeConfig.mAlertRules.GetValue();

    if (alertRules.mCPU.HasValue()) {
        if (auto err = mAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ResourceIdentifier id {ResourceLevelEnum::eSystem, ResourceTypeEnum::eCPU, {}, {}};

        if (auto err = mAlertProcessors[mAlertProcessors.Size() - 1].Init(
                id, mMaxDMIPS, alertRules.mCPU.GetValue(), *mAlertSender, CreateSystemQuotaAlertTemplate(id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mRAM.HasValue()) {
        if (auto err = mAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ResourceIdentifier id {ResourceLevelEnum::eSystem, ResourceTypeEnum::eRAM, {}, {}};

        if (auto err = mAlertProcessors[mAlertProcessors.Size() - 1].Init(
                id, mMaxMemory, alertRules.mRAM.GetValue(), *mAlertSender, CreateSystemQuotaAlertTemplate(id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& partitionRule : alertRules.mPartitions) {
        auto [totalSize, err] = GetPartitionTotalSize(partitionRule.mName);
        if (!err.IsNone()) {
            LOG_WRN() << "Failed to create alert processor for partition: name=" << partitionRule.mName
                      << ", err=" << err;

            continue;
        }

        if (err = mAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ResourceIdentifier id {ResourceLevelEnum::eSystem, ResourceTypeEnum::ePartition, partitionRule.mName, {}};

        if (err = mAlertProcessors[mAlertProcessors.Size() - 1].Init(
                id, totalSize, partitionRule, *mAlertSender, CreateSystemQuotaAlertTemplate(id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mDownload.HasValue()) {
        if (auto err = mAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ResourceIdentifier id {ResourceLevelEnum::eSystem, ResourceTypeEnum::eDownload, {}, {}};

        if (auto err = mAlertProcessors[mAlertProcessors.Size() - 1].Init(
                id, alertRules.mDownload.GetValue(), *mAlertSender, CreateSystemQuotaAlertTemplate(id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mUpload.HasValue()) {
        if (auto err = mAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ResourceIdentifier id {ResourceLevelEnum::eSystem, ResourceTypeEnum::eUpload, {}, {}};

        if (auto err = mAlertProcessors[mAlertProcessors.Size() - 1].Init(
                id, alertRules.mUpload.GetValue(), *mAlertSender, CreateSystemQuotaAlertTemplate(id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::SetupInstanceAlerts(const String& instanceID, const InstanceMonitorParams& instanceParams)
{
    LOG_DBG() << "Setup instance alerts: instanceID=" << instanceID;

    if (mInstanceAlertProcessors.Find(instanceID) != mInstanceAlertProcessors.end()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eAlreadyExist, "instance alerts processor already started"));
    }

    auto instanceAlertProcessor = MakeUnique<AlertProcessorStaticArray>(&mAllocator);

    if (auto err = mInstanceAlertProcessors.Set(instanceID, *instanceAlertProcessor); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    const auto& alertRules      = instanceParams.mAlertRules.GetValue();
    const auto& instanceIdent   = instanceParams.mInstanceIdent;
    auto&       alertProcessors = mInstanceAlertProcessors.Find(instanceID)->mSecond;

    if (alertRules.mCPU.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ResourceIdentifier id {ResourceLevelEnum::eInstance, ResourceTypeEnum::eCPU, {}, {instanceID}};

        if (auto err = alertProcessors[alertProcessors.Size() - 1].Init(id, mMaxDMIPS, alertRules.mCPU.GetValue(),
                *mAlertSender, CreateInstanceQuotaAlertTemplate(instanceIdent, id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mRAM.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ResourceIdentifier id {ResourceLevelEnum::eInstance, ResourceTypeEnum::eRAM, {}, {instanceID}};

        if (auto err = alertProcessors[alertProcessors.Size() - 1].Init(id, mMaxMemory, alertRules.mRAM.GetValue(),
                *mAlertSender, CreateInstanceQuotaAlertTemplate(instanceIdent, id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& partitionRule : alertRules.mPartitions) {
        auto [totalSize, err] = GetPartitionTotalSize(partitionRule.mName);
        if (!err.IsNone()) {
            LOG_WRN() << "Failed to create alert processor for partition: name=" << partitionRule.mName
                      << ", err=" << err;

            continue;
        }

        if (err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ResourceIdentifier id {
            ResourceLevelEnum::eInstance, ResourceTypeEnum::ePartition, partitionRule.mName, {instanceID}};

        if (err = alertProcessors[alertProcessors.Size() - 1].Init(
                id, totalSize, partitionRule, *mAlertSender, CreateInstanceQuotaAlertTemplate(instanceIdent, id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mDownload.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ResourceIdentifier id {ResourceLevelEnum::eInstance, ResourceTypeEnum::eDownload, {}, {instanceID}};

        if (auto err = alertProcessors[alertProcessors.Size() - 1].Init(id, alertRules.mDownload.GetValue(),
                *mAlertSender, CreateInstanceQuotaAlertTemplate(instanceIdent, id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mUpload.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ResourceIdentifier id {ResourceLevelEnum::eInstance, ResourceTypeEnum::eDownload, {}, {instanceID}};

        if (auto err = alertProcessors[alertProcessors.Size() - 1].Init(
                id, alertRules.mUpload.GetValue(), *mAlertSender, CreateInstanceQuotaAlertTemplate(instanceIdent, id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

void ResourceMonitor::ProcessMonitoring()
{
    while (true) {
        UniqueLock lock {mMutex};

        mCondVar.Wait(lock, mConfig.mPollPeriod, [this] { return mFinishMonitoring; });

        if (mFinishMonitoring) {
            break;
        }

        mNodeMonitoringData.mTimestamp = Time::Now();

        if (auto err = mResourceUsageProvider->GetNodeMonitoringData(
                mNodeMonitoringData.mNodeID, mNodeMonitoringData.mMonitoringData);
            !err.IsNone()) {
            LOG_ERR() << "Failed to get node monitoring data: " << err;
        }

        mNodeMonitoringData.mMonitoringData.mCPU = CPUToDMIPs(mNodeMonitoringData.mMonitoringData.mCPU);

        mNodeMonitoringData.mServiceInstances.Clear();

        for (auto& [instanceID, instanceMonitoringData] : mInstanceMonitoringData) {
            if (auto err = mResourceUsageProvider->GetInstanceMonitoringData(instanceID, instanceMonitoringData);
                !err.IsNone()) {
                LOG_ERR() << "Failed to get instance monitoring data: " << err;
            }

            instanceMonitoringData.mMonitoringData.mCPU = CPUToDMIPs(instanceMonitoringData.mMonitoringData.mCPU);

            if (auto it = mInstanceAlertProcessors.Find(instanceID); it != mInstanceAlertProcessors.end()) {
                ProcessAlerts(instanceMonitoringData.mMonitoringData, mNodeMonitoringData.mTimestamp, it->mSecond);
            }

            mNodeMonitoringData.mServiceInstances.PushBack(instanceMonitoringData);
        }

        if (auto err = mAverage.Update(mNodeMonitoringData); !err.IsNone()) {
            LOG_ERR() << "Failed to update average monitoring data: err=" << err;
        }

        ProcessAlerts(mNodeMonitoringData.mMonitoringData, mNodeMonitoringData.mTimestamp, mAlertProcessors);

        if (!mSendMonitoring) {
            continue;
        }

        if (auto err = mMonitorSender->SendMonitoringData(mNodeMonitoringData); !err.IsNone()) {
            LOG_ERR() << "Failed to send monitoring data: " << err;
        }
    }
}

void ResourceMonitor::ProcessAlerts(
    const MonitoringData& monitoringData, const Time& time, Array<AlertProcessor>& alertProcessors)
{
    for (auto& alertProcessor : alertProcessors) {
        auto [currentValue, err] = GetCurrentUsage(alertProcessor.GetID(), monitoringData);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to get resource usage: id=" << alertProcessor.GetID() << ", err=" << err;

            continue;
        }

        if (err = alertProcessor.CheckAlertDetection(currentValue, time); !err.IsNone()) {
            LOG_ERR() << "Failed to check alert detection: id=" << alertProcessor.GetID() << ", err=" << err;
        }
    }
}

RetWithError<uint64_t> ResourceMonitor::GetCurrentUsage(
    const ResourceIdentifier& id, const MonitoringData& monitoringData) const
{
    switch (id.mType.GetValue()) {
    case ResourceTypeEnum::eCPU:
        return static_cast<uint64_t>(monitoringData.mCPU + 0.5);
    case ResourceTypeEnum::eRAM:
        return monitoringData.mRAM;
    case ResourceTypeEnum::eDownload:
        return monitoringData.mDownload;
    case ResourceTypeEnum::eUpload:
        return monitoringData.mUpload;
    case ResourceTypeEnum::ePartition: {
        if (!id.mPartitionName.HasValue()) {
            return {0, ErrorEnum::eNotFound};
        }

        auto it = monitoringData.mPartitions.FindIf(
            [&id](const auto& partition) { return partition.mName == id.mPartitionName.GetValue(); });

        if (it == monitoringData.mPartitions.end()) {
            return {0, AOS_ERROR_WRAP(ErrorEnum::eNotFound)};
        }

        return {it->mUsedSize, ErrorEnum::eNone};
    }
    }

    return {0, ErrorEnum::eNotFound};
}

RetWithError<uint64_t> ResourceMonitor::GetPartitionTotalSize(const String& name) const
{
    auto it = mNodeMonitoringData.mMonitoringData.mPartitions.FindIf(
        [&name](const auto& partition) { return partition.mName == name; });

    if (it == mNodeMonitoringData.mMonitoringData.mPartitions.end()) {
        return {0, ErrorEnum::eNotFound};
    }

    return {it->mTotalSize, ErrorEnum::eNone};
}

} // namespace aos::monitoring
