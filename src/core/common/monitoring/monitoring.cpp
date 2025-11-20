/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "monitoring.hpp"

namespace aos::monitoring {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

AlertRulePoints ToPoints(const aos::AlertRulePercents& percents, uint64_t totalValue)
{
    AlertRulePoints points;

    points.mMinTimeout   = percents.mMinTimeout;
    points.mMinThreshold = static_cast<uint64_t>(static_cast<double>(totalValue) * percents.mMinThreshold / 100.0);
    points.mMaxThreshold = static_cast<uint64_t>(static_cast<double>(totalValue) * percents.mMaxThreshold / 100.0);

    return points;
}

Optional<AlertRulePoints> ToPoints(const Optional<aos::AlertRulePercents>& percents, uint64_t totalValue)
{
    if (!percents.HasValue()) {
        return {};
    }

    return ToPoints(percents.GetValue(), totalValue);
}

String GetParameterName(const ResourceIdentifier& id)
{
    if (id.mPartitionName.HasValue()) {
        return id.mPartitionName.GetValue();
    }

    return id.mType.ToString();
}

RetWithError<uint64_t> GetPartitionTotalSize(const NodeInfo& nodeInfo, const String& name)
{
    if (auto it = nodeInfo.mPartitions.FindIf([&name](const auto& partition) { return partition.mName == name; });
        it != nodeInfo.mPartitions.end()) {
        return {it->mTotalSize, ErrorEnum::eNone};
    }

    return {0, ErrorEnum::eNotFound};
}

RetWithError<uint64_t> GetCurrentUsage(const ResourceIdentifier& id, const MonitoringData& monitoringData)
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

        if (auto it = monitoringData.mPartitions.FindIf(
                [&id](const auto& partition) { return partition.mName == *id.mPartitionName; });
            it != monitoringData.mPartitions.end()) {
            return {it->mUsedSize, ErrorEnum::eNone};
        }

        return {0, AOS_ERROR_WRAP(ErrorEnum::eNotFound)};
    }
    }

    return {0, ErrorEnum::eNotFound};
}

void NormalizeMonitoringData(NodeMonitoringData& monitoringData)
{
    double   totalInstancesDMIPS    = 0.0;
    size_t   totalInstancesRAM      = 0;
    uint64_t totalInstancesDownload = 0;
    uint64_t totalInstancesUpload   = 0;

    auto& nodeMonitoringData = monitoringData.mMonitoringData;

    for (const auto& instanceMonitoring : monitoringData.mInstances) {
        totalInstancesDMIPS += instanceMonitoring.mMonitoringData.mCPU;
        totalInstancesRAM += instanceMonitoring.mMonitoringData.mRAM;
        totalInstancesDownload += instanceMonitoring.mMonitoringData.mDownload;
        totalInstancesUpload += instanceMonitoring.mMonitoringData.mUpload;

        for (const auto& partition : instanceMonitoring.mMonitoringData.mPartitions) {
            if (auto it = nodeMonitoringData.mPartitions.FindIf(
                    [&partition](const auto& p) { return p.mName == partition.mName; });
                it != nodeMonitoringData.mPartitions.end()) {
                it->mUsedSize = Max(it->mUsedSize, partition.mUsedSize);
            } else if (auto err = nodeMonitoringData.mPartitions.EmplaceBack(partition); !err.IsNone()) {
                LOG_ERR() << "Failed to normalize monitoring data: cannot add partition usage"
                          << Log::Field("partition", partition.mName) << Log::Field("error", err);
            }
        }
    }

    nodeMonitoringData.mCPU      = Max(nodeMonitoringData.mCPU, totalInstancesDMIPS);
    nodeMonitoringData.mRAM      = Max(nodeMonitoringData.mRAM, totalInstancesRAM);
    nodeMonitoringData.mDownload = Max(nodeMonitoringData.mDownload, totalInstancesDownload);
    nodeMonitoringData.mUpload   = Max(nodeMonitoringData.mUpload, totalInstancesUpload);
}

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Monitoring::Init(const Config& config, nodeconfig::NodeConfigProviderItf& nodeConfigProvider,
    iamclient::CurrentNodeInfoProviderItf& currentNodeInfoProvider, SenderItf& sender, alerts::SenderItf& alertSender,
    NodeMonitoringProviderItf& nodeMonitoringProvider, InstanceInfoProviderItf* instanceInfoProvider)
{
    LOG_INF() << "Initialize monitoring";

    mConfig                  = config;
    mNodeConfigProvider      = &nodeConfigProvider;
    mCurrentNodeInfoProvider = &currentNodeInfoProvider;
    mSender                  = &sender;
    mAlertSender             = &alertSender;
    mNodeMonitoringProvider  = &nodeMonitoringProvider;
    mInstanceInfoProvider    = instanceInfoProvider;

    if (auto err = mAverage.Init(mConfig.mAverageWindow.Nanoseconds() / mConfig.mPollPeriod.Nanoseconds());
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Monitoring::Start()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start monitoring";

    if (mIsRunning) {
        return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
    }

    if (auto err = mInstanceInfoProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNodeConfigProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    {
        auto nodeConfig = MakeUnique<NodeConfig>(&mAllocator);
        if (auto err = mNodeConfigProvider->GetNodeConfig(*nodeConfig); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = SetNodeAlertProcessors(nodeConfig->mAlertRules); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    {
        auto statuses = MakeUnique<InstanceStatusArray>(&mAllocator);

        if (auto err = mInstanceInfoProvider->GetInstancesStatuses(*statuses); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        HandleInstanceStatuses(*statuses);
    }

    if (auto err = mTimer.Start(
            mConfig.mPollPeriod, [this](void*) { ProcessMonitoring(); }, false);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mIsRunning = true;

    return ErrorEnum::eNone;
}

Error Monitoring::Stop()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Stop monitoring";

    if (!mIsRunning) {
        return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
    }

    mIsRunning = false;

    if (auto err = mTimer.Stop(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInstanceInfoProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Monitoring::GetAverageMonitoringData(NodeMonitoringData& monitoringData)
{
    LockGuard lock {mMutex};

    if (auto err = mAverage.GetData(monitoringData); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Monitoring::OnNodeConfigChanged(const NodeConfig& nodeConfig)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Node config changed";

    if (auto err = SetNodeAlertProcessors(nodeConfig.mAlertRules); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Monitoring::OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses)
{
    LockGuard lock {mMutex};

    HandleInstanceStatuses(statuses);
}

void Monitoring::HandleInstanceStatuses(const Array<InstanceStatus>& statuses)
{
    LOG_DBG() << "Handle instance statuses" << Log::Field("count", statuses.Size());

    for (const auto& status : statuses) {
        LOG_DBG() << "Instance statuses changed" << Log::Field("ident", static_cast<const InstanceIdent&>(status))
                  << Log::Field("state", status.mState);

        switch (status.mState.GetValue()) {
        case InstanceStateEnum::eActivating:
        case InstanceStateEnum::eActive:
            StartWatchingInstance(status);
            break;

        case InstanceStateEnum::eInactive:
        case InstanceStateEnum::eFailed:
            StopWatchingInstance(status);
            break;
        }
    }
}

void Monitoring::StartWatchingInstance(const InstanceStatus& instanceStatus)
{
    const auto& ident = static_cast<const InstanceIdent&>(instanceStatus);

    LOG_DBG() << "Start watching instance" << Log::Field("ident", ident);

    auto it = mWatchedInstances.FindIf([&ident](const auto& instance) { return instance.mIdent == ident; });

    if (it != mWatchedInstances.end()) {
        return;
    }

    if (auto err = mWatchedInstances.EmplaceBack(); !err.IsNone()) {
        LOG_ERR() << "Failed to watch instance" << Log::Field("ident", ident) << Log::Field(err);

        return;
    }

    mWatchedInstances.Back().mIdent = ident;

    if (auto err
        = mInstanceInfoProvider->GetInstanceMonitoringParams(ident, mWatchedInstances.Back().mMonitoringParams);
        !err.IsNone()) {
        if (err.Is(ErrorEnum::eNotSupported)) {
            LOG_DBG() << "Instance monitoring is not supported" << Log::Field("ident", ident);

            mWatchedInstances.PopBack();
        } else {
            LOG_ERR() << "Can't get instance monitoring params" << Log::Field("ident", ident) << Log::Field(err);
        }

        return;
    }

    if (auto err = mAverage.StartInstanceMonitoring(ident); !err.IsNone()) {
        LOG_ERR() << "Failed to start instance monitoring" << Log::Field("ident", ident) << Log::Field(err);

        mWatchedInstances.PopBack();
        return;
    }

    if (auto err
        = SetInstanceAlertProcessors(mWatchedInstances.Back().mMonitoringParams.mAlertRules, mWatchedInstances.Back());
        !err.IsNone()) {
        LOG_ERR() << "Failed to set instance alert processors" << Log::Field("ident", ident) << Log::Field(err);
    }
}

void Monitoring::StopWatchingInstance(const InstanceStatus& instanceStatus)
{
    mWatchedInstances.RemoveIf([&instanceStatus](const auto& instance) {
        return instance.mIdent == static_cast<const InstanceIdent&>(instanceStatus);
    });

    mAverage.StopInstanceMonitoring(instanceStatus);
}

void Monitoring::GetInstanceMonitoringData(Array<InstanceMonitoringData>& instanceMonitoringData)
{
    if (!mInstanceInfoProvider) {
        return;
    }

    for (auto& instance : mWatchedInstances) {
        LOG_DBG() << "Get monitoring data for instance" << Log::Field("ident", instance.mIdent);

        if (auto err = instanceMonitoringData.EmplaceBack(); !err.IsNone()) {
            LOG_ERR() << "Failed to add instance monitoring data" << Log::Field("ident", instance.mIdent)
                      << Log::Field(err);
            return;
        }

        auto& instanceData = instanceMonitoringData.Back();

        if (auto err = mInstanceInfoProvider->GetInstanceMonitoringData(instance.mIdent, instanceData); !err.IsNone()) {
            LOG_ERR() << "Failed to get instance monitoring data" << Log::Field("ident", instance.mIdent)
                      << Log::Field(err);

            instanceMonitoringData.PopBack();
            continue;
        }
    }
}

void Monitoring::ProcessAlerts(NodeMonitoringData& monitoringData)
{
    ProcessAlerts(monitoringData.mMonitoringData, mNodeAlertProcessors);

    for (auto& instanceData : monitoringData.mInstances) {
        auto it = mWatchedInstances.FindIf(
            [&instanceData](const auto& instance) { return instance.mIdent == instanceData.mInstanceIdent; });
        if (it == mWatchedInstances.end()) {
            continue;
        }

        ProcessAlerts(instanceData.mMonitoringData, it->mAlertProcessors);
    }
}

void Monitoring::ProcessAlerts(MonitoringData& monitoringData, AlertProcessorArray& alertProcessors)
{
    for (auto& alertProcessor : alertProcessors) {
        auto [currentValue, err] = GetCurrentUsage(alertProcessor.GetID(), monitoringData);
        if (!err.IsNone()) {
            LOG_ERR() << "Can't get resource usage" << Log::Field("id", alertProcessor.GetID()) << Log::Field(err);

            continue;
        }

        err = alertProcessor.CheckAlertDetection(currentValue, monitoringData.mTimestamp);
        if (!err.IsNone()) {
            LOG_ERR() << "Can't check alert detection" << Log::Field("id", alertProcessor.GetID()) << Log::Field(err);
        }
    }
}

UniquePtr<ResourceIdentifier> Monitoring::CreateResourceIdentifier(ResourceLevel level, ResourceType type,
    const Optional<StaticString<cPartitionNameLen>>& partitionName, const Optional<InstanceIdent>& instanceIdent)
{
    return MakeUnique<ResourceIdentifier>(&mAllocator, level, type, partitionName, instanceIdent);
}

void Monitoring::ProcessMonitoring()
{
    UniqueLock lock {mMutex};

    auto nodeMonitoringData        = MakeUnique<NodeMonitoringData>(&mAllocator);
    nodeMonitoringData->mTimestamp = Time::Now();

    GetInstanceMonitoringData(nodeMonitoringData->mInstances);

    if (auto err = mNodeMonitoringProvider->GetNodeMonitoringData(nodeMonitoringData->mMonitoringData); !err.IsNone()) {
        LOG_ERR() << "Can't get node monitoring data" << Log::Field(err);
    }

    if (auto err = mAverage.Update(*nodeMonitoringData); !err.IsNone()) {
        LOG_ERR() << "Failed to update average monitoring data: err=" << err;
    }

    ProcessAlerts(*nodeMonitoringData);

    NormalizeMonitoringData(*nodeMonitoringData);

    if (auto err = mSender->SendMonitoringData(*nodeMonitoringData); !err.IsNone()) {
        LOG_ERR() << "Can't send monitoring data" << Log::Field(err);
    }
}

AlertVariant Monitoring::CreateSystemQuotaAlertTemplate(const ResourceIdentifier& resourceIdentifier) const
{
    AlertVariant     alertItem;
    SystemQuotaAlert quotaAlert {};

    quotaAlert.mNodeID    = mNodeID;
    quotaAlert.mParameter = GetParameterName(resourceIdentifier);

    alertItem.SetValue<SystemQuotaAlert>(quotaAlert);

    return alertItem;
}

AlertVariant Monitoring::CreateInstanceQuotaAlertTemplate(
    const InstanceIdent& instanceIdent, const ResourceIdentifier& resourceIdentifier) const
{
    AlertVariant       alertItem;
    InstanceQuotaAlert quotaAlert {};

    static_cast<InstanceIdent&>(quotaAlert) = instanceIdent;
    quotaAlert.mParameter                   = GetParameterName(resourceIdentifier);

    alertItem.SetValue<InstanceQuotaAlert>(quotaAlert);

    return alertItem;
}

Error Monitoring::SetNodeAlertProcessors(const Optional<aos::AlertRules>& alertRules)
{
    LOG_DBG() << "Setup system alerts";

    mNodeAlertProcessors.Clear();

    if (!alertRules.HasValue()) {
        return ErrorEnum::eNone;
    }

    auto nodeInfo = MakeUnique<NodeInfo>(&mAllocator);

    if (auto err = mCurrentNodeInfoProvider->GetCurrentNodeInfo(*nodeInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNodeID.Assign(nodeInfo->mNodeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (const auto cpu = ToPoints(alertRules->mCPU, nodeInfo->mMaxDMIPS); cpu.HasValue()) {
        if (auto err = mNodeAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eSystem, ResourceTypeEnum::eCPU);

        if (auto err = mNodeAlertProcessors.Back().Init(*id, *cpu, *mAlertSender, CreateSystemQuotaAlertTemplate(*id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (const auto ram = ToPoints(alertRules->mRAM, nodeInfo->mTotalRAM); ram.HasValue()) {
        if (auto err = mNodeAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eSystem, ResourceTypeEnum::eRAM);

        if (auto err = mNodeAlertProcessors.Back().Init(*id, *ram, *mAlertSender, CreateSystemQuotaAlertTemplate(*id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules->mDownload.HasValue()) {
        if (auto err = mNodeAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eSystem, ResourceTypeEnum::eDownload);

        if (auto err = mNodeAlertProcessors.Back().Init(
                *id, *alertRules->mDownload, *mAlertSender, CreateSystemQuotaAlertTemplate(*id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules->mUpload.HasValue()) {
        if (auto err = mNodeAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eSystem, ResourceTypeEnum::eUpload);

        if (auto err = mNodeAlertProcessors.Back().Init(
                *id, *alertRules->mUpload, *mAlertSender, CreateSystemQuotaAlertTemplate(*id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& partition : alertRules->mPartitions) {
        auto [totalSize, err] = GetPartitionTotalSize(*nodeInfo, partition.mName);
        if (!err.IsNone()) {
            LOG_WRN() << "Can't initialize partition alert processor" << Log::Field("name", partition.mName)
                      << Log::Field(err);

            continue;
        }

        if (err = mNodeAlertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eSystem, ResourceTypeEnum::ePartition, partition.mName);

        if (err = mNodeAlertProcessors.Back().Init(
                *id, ToPoints(partition, totalSize), *mAlertSender, CreateSystemQuotaAlertTemplate(*id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Monitoring::SetInstanceAlertProcessors(const Optional<AlertRules>& alertRules, Instance& instance)
{
    const auto& ident = instance.mIdent;

    LOG_DBG() << "Setup instance alerts" << Log::Field("ident", ident);

    auto& alertProcessors = instance.mAlertProcessors;

    alertProcessors.Clear();

    if (!alertRules.HasValue()) {
        return ErrorEnum::eNone;
    }

    if (alertRules->mCPU.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eInstance, ResourceTypeEnum::eCPU, {}, {ident});

        if (auto err = alertProcessors.Back().Init(
                *id, *alertRules->mCPU, *mAlertSender, CreateInstanceQuotaAlertTemplate(ident, *id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules->mRAM.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eInstance, ResourceTypeEnum::eRAM, {}, {ident});

        if (auto err = alertProcessors.Back().Init(
                *id, *alertRules->mRAM, *mAlertSender, CreateInstanceQuotaAlertTemplate(ident, *id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules->mDownload.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eInstance, ResourceTypeEnum::eDownload, {}, {ident});

        if (auto err = alertProcessors.Back().Init(
                *id, *alertRules->mDownload, *mAlertSender, CreateInstanceQuotaAlertTemplate(ident, *id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules->mUpload.HasValue()) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(ResourceLevelEnum::eInstance, ResourceTypeEnum::eDownload, {}, {ident});

        if (auto err = alertProcessors.Back().Init(
                *id, *alertRules->mUpload, *mAlertSender, CreateInstanceQuotaAlertTemplate(ident, *id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& partition : alertRules->mPartitions) {
        if (auto err = alertProcessors.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto id = CreateResourceIdentifier(
            ResourceLevelEnum::eInstance, ResourceTypeEnum::ePartition, partition.mName, {ident});

        if (auto err
            = alertProcessors.Back().Init(*id, partition, *mAlertSender, CreateInstanceQuotaAlertTemplate(ident, *id));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::monitoring
