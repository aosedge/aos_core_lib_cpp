/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "resourcemonitor.hpp"

namespace aos::monitoring {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

// cppcheck-suppress constParameterReference
Error ResourceMonitor::Init(const Config& config, const iam::nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    sm::resourcemanager::ResourceManagerItf& resourceManager, ResourceUsageProviderItf& resourceUsageProvider,
    SenderItf& monitorSender, alerts::SenderItf& alertSender, cloudconnection::CloudConnectionItf& cloudConnection)
{
    LOG_DBG() << "Init resource monitor";

    mConfig                = config;
    mResourceUsageProvider = &resourceUsageProvider;
    mResourceManager       = &resourceManager;
    mMonitorSender         = &monitorSender;
    mAlertSender           = &alertSender;
    mCloudConnection       = &cloudConnection;

    auto nodeInfo = MakeUnique<NodeInfoObsolete>(&mAllocator);

    if (auto err = nodeInfoProvider.GetNodeInfo(*nodeInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = InitPartitions(*nodeInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mNodeMonitoringData.mNodeID = nodeInfo->mNodeID;
    mMaxDMIPS                   = nodeInfo->mMaxDMIPS;
    mMaxMemory                  = nodeInfo->mTotalRAM;

    assert(mConfig.mPollPeriod > 0);

    if (auto err
        = mAverage.Init(mPartitionInfos, mConfig.mAverageWindow.Nanoseconds() / mConfig.mPollPeriod.Nanoseconds());
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::Start()
{
    LOG_DBG() << "Start monitoring";

    if (auto err = mCloudConnection->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto nodeConfig = MakeUnique<sm::resourcemanager::NodeConfig>(&mAllocator);

    if (auto err = mResourceManager->GetNodeConfig(*nodeConfig); !err.IsNone()) {
        LOG_ERR() << "Get node config failed, err=" << AOS_ERROR_WRAP(err);
    }

    if (auto err = mResourceManager->SubscribeCurrentNodeConfigChange(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetupSystemAlerts(*nodeConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mTimer.Start(
            mConfig.mPollPeriod, [this](void*) { ProcessMonitoring(); }, false);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ResourceMonitor::Stop()
{
    LOG_DBG() << "Stop monitoring";

    mTimer.Stop();
    mCloudConnection->UnsubscribeListener(*this);

    if (auto err = mResourceManager->UnsubscribeCurrentNodeConfigChange(*this); !err.IsNone()) {
        LOG_ERR() << "Unsubscription on node config change failed" << Log::Field(AOS_ERROR_WRAP(err));
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

Error ResourceMonitor::ReceiveNodeConfig(const sm::resourcemanager::NodeConfig& nodeConfig)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Receive node config: version=" << nodeConfig.mVersion;

    if (auto err = SetupSystemAlerts(nodeConfig); !err.IsNone()) {
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

    // cppcheck-suppress constParameterPointer
    auto cleanUp = DeferRelease(&err, [this, &instanceID](Error* err) {
        if (!err->IsNone()) {
            mInstanceMonitoringData.Remove(instanceID);
            mInstanceAlertProcessors.Remove(instanceID);
        }
    });

    if (err = mResourceUsageProvider->GetInstanceMonitoringData(
            instanceID, mInstanceMonitoringData.Find(instanceID)->mSecond);
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
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

Error ResourceMonitor::UpdateInstanceState(const String& instanceID, InstanceState state)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Update instance state: instanceID=" << instanceID << ", state=" << state;

    auto instanceData = mInstanceMonitoringData.Find(instanceID);
    if (instanceData == mInstanceMonitoringData.end()) {
        return ErrorEnum::eNotFound;
    }

    instanceData->mSecond.mState = state;

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

    if (auto err = mAverage.StopInstanceMonitoring(instanceData->mSecond.mInstanceIdent);
        !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    mInstanceMonitoringData.Erase(instanceData);

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

Error ResourceMonitor::InitPartitions(const NodeInfoObsolete& nodeInfo)
{
    for (const auto& partition : nodeInfo.mPartitions) {
        if (auto err = mPartitionInfos.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& partitionInfo = mPartitionInfos.Back();

        partitionInfo.mTotalSize = partition.mTotalSize;

        if (auto err = partitionInfo.mName.Assign(partition.mName); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = partitionInfo.mPath.Assign(partition.mPath); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = partitionInfo.mTypes.Assign(partition.mTypes); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

String ResourceMonitor::GetParameterName(const ResourceIdentifier& id) const
{
    if (id.mPartitionName.HasValue()) {
        return id.mPartitionName.GetValue();
    }

    return id.mType.ToString();
}

AlertVariant ResourceMonitor::CreateSystemQuotaAlertTemplate(const ResourceIdentifier& resourceIdentifier) const
{
    AlertVariant     alertItem;
    SystemQuotaAlert quotaAlert {};

    quotaAlert.mNodeID    = mNodeMonitoringData.mNodeID;
    quotaAlert.mParameter = GetParameterName(resourceIdentifier);

    alertItem.SetValue<SystemQuotaAlert>(quotaAlert);

    return alertItem;
}

AlertVariant ResourceMonitor::CreateInstanceQuotaAlertTemplate(
    const InstanceIdent& instanceIdent, const ResourceIdentifier& resourceIdentifier) const
{
    AlertVariant       alertItem;
    InstanceQuotaAlert quotaAlert {};

    static_cast<InstanceIdent&>(quotaAlert) = instanceIdent;
    quotaAlert.mParameter                   = GetParameterName(resourceIdentifier);

    alertItem.SetValue<InstanceQuotaAlert>(quotaAlert);

    return alertItem;
}

double ResourceMonitor::CPUToDMIPs(double cpuPersentage) const
{
    return cpuPersentage * static_cast<double>(mMaxDMIPS) / 100.0;
}

Error ResourceMonitor::SetupSystemAlerts(const sm::resourcemanager::NodeConfig& nodeConfig)
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

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eSystem, ResourceTypeEnum::eCPU);

        if (auto err = mAlertProcessors[mAlertProcessors.Size() - 1].Init(
                *id, mMaxDMIPS, alertRules.mCPU.GetValue(), *mAlertSender, CreateSystemQuotaAlertTemplate(*id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mRAM.HasValue()) {
        if (auto err = mAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eSystem, ResourceTypeEnum::eRAM);

        if (auto err = mAlertProcessors[mAlertProcessors.Size() - 1].Init(
                *id, mMaxMemory, alertRules.mRAM.GetValue(), *mAlertSender, CreateSystemQuotaAlertTemplate(*id));
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

        auto id
            = CreateResourceIdentifier(ResourceLevelEnum::eSystem, ResourceTypeEnum::ePartition, partitionRule.mName);

        if (err = mAlertProcessors[mAlertProcessors.Size() - 1].Init(
                *id, totalSize, partitionRule, *mAlertSender, CreateSystemQuotaAlertTemplate(*id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mDownload.HasValue()) {
        if (auto err = mAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eSystem, ResourceTypeEnum::eDownload);

        if (auto err = mAlertProcessors[mAlertProcessors.Size() - 1].Init(
                *id, alertRules.mDownload.GetValue(), *mAlertSender, CreateSystemQuotaAlertTemplate(*id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mUpload.HasValue()) {
        if (auto err = mAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eSystem, ResourceTypeEnum::eUpload);

        if (auto err = mAlertProcessors[mAlertProcessors.Size() - 1].Init(
                *id, alertRules.mUpload.GetValue(), *mAlertSender, CreateSystemQuotaAlertTemplate(*id));
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

    auto instanceAlertProcessor = MakeUnique<AlertProcessorArray>(&mAllocator);

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

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eInstance, ResourceTypeEnum::eCPU, {}, {instanceID});

        if (auto err = alertProcessors[alertProcessors.Size() - 1].Init(*id, mMaxDMIPS, alertRules.mCPU.GetValue(),
                *mAlertSender, CreateInstanceQuotaAlertTemplate(instanceIdent, *id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mRAM.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eInstance, ResourceTypeEnum::eRAM, {}, {instanceID});

        if (auto err = alertProcessors[alertProcessors.Size() - 1].Init(*id, mMaxMemory, alertRules.mRAM.GetValue(),
                *mAlertSender, CreateInstanceQuotaAlertTemplate(instanceIdent, *id));
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

        auto id = CreateResourceIdentifier(
            ResourceLevelEnum::eInstance, ResourceTypeEnum::ePartition, partitionRule.mName, {instanceID});

        if (err = alertProcessors[alertProcessors.Size() - 1].Init(
                *id, totalSize, partitionRule, *mAlertSender, CreateInstanceQuotaAlertTemplate(instanceIdent, *id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mDownload.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eInstance, ResourceTypeEnum::eDownload, {}, {instanceID});

        if (auto err = alertProcessors[alertProcessors.Size() - 1].Init(*id, alertRules.mDownload.GetValue(),
                *mAlertSender, CreateInstanceQuotaAlertTemplate(instanceIdent, *id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mUpload.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eInstance, ResourceTypeEnum::eDownload, {}, {instanceID});

        if (auto err = alertProcessors[alertProcessors.Size() - 1].Init(*id, alertRules.mUpload.GetValue(),
                *mAlertSender, CreateInstanceQuotaAlertTemplate(instanceIdent, *id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

void ResourceMonitor::NormalizeMonitoringData()
{
    double   totalInstancesDMIPS    = 0.0;
    size_t   totalInstancesRAM      = 0;
    uint64_t totalInstancesDownload = 0;
    uint64_t totalInstancesUpload   = 0;

    auto& nodeMonitoringData = mNodeMonitoringData.mMonitoringData;

    for (const auto& instanceMonitoring : mNodeMonitoringData.mServiceInstances) {
        totalInstancesDMIPS += instanceMonitoring.mMonitoringData.mCPU;
        totalInstancesRAM += instanceMonitoring.mMonitoringData.mRAM;
        totalInstancesDownload += instanceMonitoring.mMonitoringData.mDownload;
        totalInstancesUpload += instanceMonitoring.mMonitoringData.mUpload;

        for (const auto& partition : instanceMonitoring.mMonitoringData.mPartitions) {
            if (auto it = nodeMonitoringData.mPartitions.FindIf(
                    [&partition](const auto& p) { return p.mName == partition.mName; });
                it != nodeMonitoringData.mPartitions.end()) {
                it->mUsedSize = Max(it->mUsedSize, partition.mUsedSize);
            }
        }
    }

    nodeMonitoringData.mCPU      = Max(nodeMonitoringData.mCPU, totalInstancesDMIPS);
    nodeMonitoringData.mRAM      = Max(nodeMonitoringData.mRAM, totalInstancesRAM);
    nodeMonitoringData.mDownload = Max(nodeMonitoringData.mDownload, totalInstancesDownload);
    nodeMonitoringData.mUpload   = Max(nodeMonitoringData.mUpload, totalInstancesUpload);
}

void ResourceMonitor::ProcessMonitoring()
{
    UniqueLock lock {mMutex};

    mNodeMonitoringData.mTimestamp = Time::Now();

    mNodeMonitoringData.mMonitoringData.mPartitions.Clear();
    mNodeMonitoringData.mServiceInstances.Clear();

    for (auto& [instanceID, instanceMonitoringData] : mInstanceMonitoringData) {
        if (auto err = mResourceUsageProvider->GetInstanceMonitoringData(instanceID, instanceMonitoringData);
            !err.IsNone()) {
            if (instanceMonitoringData.mState == InstanceStateEnum::eActive) {
                LOG_ERR() << "Failed to get instance monitoring data: err=" << err;
            }

            continue;
        }

        instanceMonitoringData.mMonitoringData.mCPU       = CPUToDMIPs(instanceMonitoringData.mMonitoringData.mCPU);
        instanceMonitoringData.mMonitoringData.mTimestamp = mNodeMonitoringData.mTimestamp;

        if (auto it = mInstanceAlertProcessors.Find(instanceID); it != mInstanceAlertProcessors.end()) {
            ProcessAlerts(instanceMonitoringData.mMonitoringData, mNodeMonitoringData.mTimestamp, it->mSecond);
        }

        if (auto err = mNodeMonitoringData.mServiceInstances.PushBack(instanceMonitoringData); !err.IsNone()) {
            LOG_ERR() << "Failed to add instance monitoring data"
                      << Log::Field("instanceIdent", instanceMonitoringData.mInstanceIdent) << Log::Field(err);
        }
    }

    if (auto err = mResourceUsageProvider->GetNodeMonitoringData(mPartitionInfos, mNodeMonitoringData.mMonitoringData);
        !err.IsNone()) {
        LOG_ERR() << "Failed to get node monitoring data: err=" << err;
    }

    mNodeMonitoringData.mMonitoringData.mCPU = CPUToDMIPs(mNodeMonitoringData.mMonitoringData.mCPU);

    if (auto err = mAverage.Update(mNodeMonitoringData); !err.IsNone()) {
        LOG_ERR() << "Failed to update average monitoring data: err=" << err;
    }

    ProcessAlerts(mNodeMonitoringData.mMonitoringData, mNodeMonitoringData.mTimestamp, mAlertProcessors);

    if (!mSendMonitoring) {
        return;
    }

    NormalizeMonitoringData();

    if (auto err = mMonitorSender->SendMonitoringData(mNodeMonitoringData); !err.IsNone()) {
        LOG_ERR() << "Failed to send monitoring data: " << err;
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
    if (auto it = mPartitionInfos.FindIf([&name](const auto& partition) { return partition.mName == name; });
        it != mPartitionInfos.end()) {
        return {it->mTotalSize, ErrorEnum::eNone};
    }

    return {0, ErrorEnum::eNotFound};
}

UniquePtr<ResourceIdentifier> ResourceMonitor::CreateResourceIdentifier(ResourceLevel level, ResourceType type,
    const Optional<StaticString<cPartitionNameLen>>& partitionName,
    const Optional<StaticString<cIDLen>>&            instanceID) const
{
    return MakeUnique<ResourceIdentifier>(&mAllocator, level, type, partitionName, instanceID);
}

} // namespace aos::monitoring
