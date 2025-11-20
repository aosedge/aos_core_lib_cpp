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

AlertRulePoints ToPoints(const AlertRulePercents& percents, uint64_t totalValue)
{
    AlertRulePoints points;

    points.mMinTimeout   = percents.mMinTimeout;
    points.mMinThreshold = static_cast<uint64_t>(static_cast<double>(totalValue) * percents.mMinThreshold / 100.0);
    points.mMaxThreshold = static_cast<uint64_t>(static_cast<double>(totalValue) * percents.mMaxThreshold / 100.0);

    return points;
}

Optional<AlertRulePoints> ToPoints(const Optional<AlertRulePercents>& percents, uint64_t totalValue)
{
    if (!percents.HasValue()) {
        return {};
    }

    return ToPoints(*percents, totalValue);
}

String GetParameterName(const ResourceIdentifier& id)
{
    if (id.mPartitionName.HasValue()) {
        return id.mPartitionName.GetValue();
    }

    return id.mType.ToString();
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

    if (auto err = mCurrentNodeInfoProvider->GetCurrentNodeInfo(mNodeInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mInstanceInfoProvider) {
        auto statuses = MakeUnique<InstanceStatusArray>(&mAllocator);

        if (auto err = mInstanceInfoProvider->GetInstancesStatuses(*statuses); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        HandleInstanceStatuses(*statuses);

        if (auto err = mInstanceInfoProvider->SubscribeListener(*this); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        LOG_WRN() << "Instance monitoring is not supported";
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

    Error err;

    if (auto stopErr = mTimer.Stop(); err.IsNone() && !stopErr.IsNone()) {
        err = AOS_ERROR_WRAP(stopErr);
    }

    if (auto unsubscribeErr = mNodeConfigProvider->UnsubscribeListener(*this);
        err.IsNone() && !unsubscribeErr.IsNone()) {
        err = AOS_ERROR_WRAP(unsubscribeErr);
    }

    if (mInstanceInfoProvider) {
        if (auto unsubscribeErr = mInstanceInfoProvider->UnsubscribeListener(*this);
            err.IsNone() && !unsubscribeErr.IsNone()) {
            err = AOS_ERROR_WRAP(unsubscribeErr);
        }
    }

    return err;
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

    if (auto err = SetInstanceAlertProcessors(mWatchedInstances.Back().mMonitoringParams, mWatchedInstances.Back());
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

Error Monitoring::CreateAlertTemplate(const ResourceIdentifier& resourceIdentifier, AlertVariant& alert) const
{
    switch (resourceIdentifier.mLevel.GetValue()) {
    case ResourceLevelEnum::eSystem: {
        SystemQuotaAlert quotaAlert {};

        quotaAlert.mNodeID    = mNodeInfo.mNodeID;
        quotaAlert.mParameter = GetParameterName(resourceIdentifier);

        alert.SetValue<SystemQuotaAlert>(quotaAlert);

        return ErrorEnum::eNone;
    }

    case ResourceLevelEnum::eInstance: {
        if (!resourceIdentifier.mInstanceIdent.HasValue()) {
            return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
        }

        InstanceQuotaAlert quotaAlert {};

        static_cast<InstanceIdent&>(quotaAlert) = *resourceIdentifier.mInstanceIdent;
        quotaAlert.mParameter                   = GetParameterName(resourceIdentifier);

        alert.SetValue<InstanceQuotaAlert>(quotaAlert);

        return ErrorEnum::eNone;
    }
    }

    return AOS_ERROR_WRAP(ErrorEnum::eNotSupported);
}

Error Monitoring::AddAlertProcessor(
    const AlertRulePoints& rule, const ResourceIdentifier& identifier, Array<AlertProcessor>& processors)
{
    if (auto err = processors.EmplaceBack(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto& alertProcessor = processors.Back();

    AlertVariant alertTemplate;

    if (auto err = CreateAlertTemplate(identifier, alertTemplate); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = alertProcessor.Init(identifier, rule, *mAlertSender, alertTemplate); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Monitoring::SetNodeAlertProcessors(const Optional<aos::AlertRules>& alertRules)
{
    LOG_DBG() << "Setup system alerts";

    mNodeAlertProcessors.Clear();

    if (!alertRules.HasValue()) {
        return ErrorEnum::eNone;
    }
    return SetAlertProcessors(*alertRules, ResourceLevelEnum::eSystem, {}, mNodeAlertProcessors);
}

Error Monitoring::SetInstanceAlertProcessors(const Optional<InstanceMonitoringParams>& params, Instance& instance)
{
    const auto& ident = instance.mIdent;

    LOG_DBG() << "Setup instance alerts" << Log::Field("ident", ident);

    auto& alertProcessors = instance.mAlertProcessors;

    alertProcessors.Clear();

    if (!params.HasValue() || !params->mAlertRules.HasValue()) {

        return ErrorEnum::eNone;
    }

    return SetAlertProcessors(
        params->mAlertRules.GetValue(), ResourceLevelEnum::eInstance, Optional<InstanceIdent>(ident), alertProcessors);
}

Error Monitoring::SetAlertProcessors(const AlertRules& alertRules, const ResourceLevel& level,
    const Optional<InstanceIdent>& instanceIdent, Array<AlertProcessor>& processors)
{
    if (auto cpu = ToPoints(alertRules.mCPU, mNodeInfo.mMaxDMIPS); cpu.HasValue()) {
        auto id = ResourceIdentifier(level, ResourceTypeEnum::eCPU, {}, instanceIdent);

        if (auto err = AddAlertProcessor(*cpu, id, processors); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto ram = ToPoints(alertRules.mRAM, mNodeInfo.mTotalRAM); ram.HasValue()) {
        auto id = ResourceIdentifier(level, ResourceTypeEnum::eRAM, {}, instanceIdent);

        if (auto err = AddAlertProcessor(*ram, id, processors); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mDownload.HasValue()) {
        auto id = ResourceIdentifier(level, ResourceTypeEnum::eDownload, {}, instanceIdent);

        if (auto err = AddAlertProcessor(*alertRules.mDownload, id, processors); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (alertRules.mUpload.HasValue()) {
        auto id = ResourceIdentifier(level, ResourceTypeEnum::eUpload, {}, instanceIdent);

        if (auto err = AddAlertProcessor(*alertRules.mUpload, id, processors); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& partition : alertRules.mPartitions) {
        auto it = mNodeInfo.mPartitions.FindIf([&partition](const auto& p) { return p.mName == partition.mName; });
        if (it == mNodeInfo.mPartitions.end()) {
            LOG_WRN() << "Partition not found for alert rule" << Log::Field("partition", partition.mName);
            continue;
        }

        auto id = ResourceIdentifier(level, ResourceTypeEnum::ePartition, partition.mName, instanceIdent);

        if (auto err = AddAlertProcessor(ToPoints(partition, it->mTotalSize), id, processors); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::monitoring
