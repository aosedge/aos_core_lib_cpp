/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "node.hpp"

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Node::Init(
    const UnitNodeInfo& info, unitconfig::NodeConfigProviderItf& nodeConfigProvider, InstanceRunnerItf& instanceRunner)
{
    mNodeConfigProvider = &nodeConfigProvider;
    mInstanceRunner     = &instanceRunner;

    UpdateInfo(info);

    return ErrorEnum::eNone;
}

void Node::UpdateAvailableResources(const monitoring::NodeMonitoringData& monitoringData, bool rebalancing)
{
    mRuntimeAvailableCPU.Clear();
    mRuntimeAvailableRAM.Clear();
    mMaxInstances.Clear();

    mNeedBalancing = false;

    if (rebalancing && mConfig.mAlertRules.HasValue()) {
        const auto& alertRules = mConfig.mAlertRules.GetValue();
        if (alertRules.mCPU.HasValue() || alertRules.mRAM.HasValue()) {
            if (alertRules.mCPU.HasValue()) {
                const auto usedCPU = monitoringData.mMonitoringData.mCPU;
                const auto maxTreshold
                    = mInfo.mMaxDMIPS * static_cast<size_t>(alertRules.mCPU.GetValue().mMaxThreshold / 100.0);

                if (usedCPU > maxTreshold) {
                    mNeedBalancing = true;
                }
            }

            if (alertRules.mRAM.HasValue()) {
                const auto usedRAM = monitoringData.mMonitoringData.mRAM;
                const auto maxTreshold
                    = mInfo.mMaxDMIPS * static_cast<size_t>(alertRules.mRAM.GetValue().mMaxThreshold / 100.0);

                if (usedRAM > maxTreshold) {
                    mNeedBalancing = true;
                }
            }
        }
    }

    auto systemCPUUsage = GetSystemCPUUsage(monitoringData);
    auto systemRAMUsage = GetSystemRAMUsage(monitoringData);
    auto totalCPU       = mInfo.mMaxDMIPS;
    auto totalRAM       = mInfo.mTotalRAM;

    // For nodes requiring rebalancing, we need to decrease resource consumption below the low threshold.
    if (mNeedBalancing) {
        if (mConfig.mAlertRules.HasValue()) {
            const auto& alertRules = mConfig.mAlertRules.GetValue();
            if (alertRules.mCPU.HasValue()) {
                totalCPU = static_cast<size_t>(mInfo.mMaxDMIPS * alertRules.mCPU.GetValue().mMinThreshold / 100.0);
            }

            if (alertRules.mRAM.HasValue()) {
                totalRAM = static_cast<size_t>(mInfo.mTotalRAM * alertRules.mRAM.GetValue().mMinThreshold / 100.0);
            }
        }
    }

    mAvailableCPU       = systemCPUUsage > totalCPU ? 0 : totalCPU - systemCPUUsage;
    mAvailableRAM       = systemRAMUsage > totalRAM ? 0 : totalRAM - systemRAMUsage;
    mAvailableResources = mInfo.mResources;

    if (mNeedBalancing) {
        LOG_DBG() << "Node resource usage" << Log::Field("nodeID", mInfo.mNodeID) << Log::Field("RAM", systemRAMUsage)
                  << Log::Field("CPU", systemCPUUsage);
    }

    LOG_DBG() << "Available resources" << Log::Field("nodeID", mInfo.mNodeID) << Log::Field("RAM", mAvailableRAM)
              << Log::Field("CPU", mAvailableCPU);
}

bool Node::UpdateInfo(const UnitNodeInfo& info)
{
    // Ignore connection status.
    mInfo.mIsConnected = info.mIsConnected;

    bool nodeChanged = mInfo != info;
    if (nodeChanged) {
        mInfo = info;
        UpdateConfig();
    }

    return nodeChanged;
}

Error Node::LoadSentInstances(const Array<SharedPtr<Instance>>& instances)
{
    mSentInstances.Clear();

    for (const auto& instance : instances) {
        if (instance->GetStatus().mNodeID == mInfo.mNodeID) {
            if (auto err = mSentInstances.EmplaceBack(); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            static_cast<InstanceIdent&>(mSentInstances.Back()) = instance->GetInfo().mInstanceIdent;
            mSentInstances.Back().mRuntimeID                   = instance->GetStatus().mRuntimeID;
        }
    }

    return ErrorEnum::eNone;
}

Error Node::UpdateRunningInstances(const Array<InstanceStatus>& instances)
{
    if (auto err = mRunningInstances.Assign(instances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

size_t Node::GetAvailableCPU()
{
    return mAvailableCPU;
}

size_t Node::GetAvailableRAM()
{
    return mAvailableRAM;
}

size_t Node::GetAvailableCPU(const String& runtimeID)
{
    const auto* ptr = GetPtrToAvailableCPU(runtimeID);

    return ptr == nullptr ? 0 : *ptr;
}

size_t Node::GetAvailableRAM(const String& runtimeID)
{
    const auto* ptr = GetPtrToAvailableRAM(runtimeID);

    return ptr == nullptr ? 0 : *ptr;
}

bool Node::IsMaxNumInstancesReached(const String& runtimeID)
{
    const auto* ptr = GetPtrToMaxNumInstances(runtimeID);

    return ptr == nullptr || *ptr == 0;
}

void Node::UpdateConfig()
{
    if (auto err = mNodeConfigProvider->GetNodeConfig(mInfo.mNodeID, mInfo.mNodeType, mConfig); !err.IsNone()) {
        LOG_ERR() << "Get node config failed" << Log::Field("nodeID", mInfo.mNodeID) << Log::Field(AOS_ERROR_WRAP(err));
    }
}

Error Node::ScheduleInstance(const aos::InstanceInfo& instance, const networkmanager::NetworkServiceData& servData,
    size_t reqCPU, size_t reqRAM, const Array<StaticString<cResourceNameLen>>& reqResources)
{
    if (auto err = mScheduledInstances.PushBack(instance); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNetworkServiceData.PushBack(servData); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // adjust available resources
    auto availableRAM = GetPtrToAvailableRAM(instance.mRuntimeID);
    if (availableRAM == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    *availableRAM = *availableRAM < reqRAM ? 0 : *availableRAM - reqRAM;

    auto availableCPU = GetPtrToAvailableCPU(instance.mRuntimeID);
    if (availableCPU == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    *availableCPU = *availableCPU < reqCPU ? 0 : *availableCPU - reqCPU;

    auto maxNumInstances = GetPtrToMaxNumInstances(instance.mRuntimeID);
    if (maxNumInstances == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (*maxNumInstances == 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    (*maxNumInstances)--;

    for (const auto& resource : reqResources) {
        auto availableResource
            = mAvailableResources.FindIf([&resource](const ResourceInfo& info) { return info.mName == resource; });

        if (availableResource == nullptr || availableResource->mSharedCount < 1) {
            return AOS_ERROR_WRAP(ErrorEnum::eFailed);
        }

        availableResource->mSharedCount--;
    }

    LOG_INF() << "Available CPU & RAM" << Log::Field("nodeID", mInfo.mNodeID) << Log::Field("CPU", *availableCPU)
              << Log::Field("RAM", *availableRAM);

    return ErrorEnum::eNone;
}

Error Node::SetupNetworkParams(
    bool onlyExposedPorts, networkmanager::NetworkManagerItf& netMgr, Array<SharedPtr<Instance>>& instances)
{
    for (size_t i = 0; i < mScheduledInstances.Size();) {
        const auto& scheduledInstance = mScheduledInstances[i];

        if (onlyExposedPorts && mNetworkServiceData[i].mExposedPorts.IsEmpty()) {
            ++i;

            continue;
        }

        auto netErr = netMgr.PrepareInstanceNetworkParameters(scheduledInstance, mScheduledInstances[i].mOwnerID,
            mInfo.mNodeID, mNetworkServiceData[i], mScheduledInstances[i].mNetworkParameters.GetValue());
        if (!netErr.IsNone()) {
            auto instance = instances.FindIf([&scheduledInstance](const SharedPtr<Instance>& instance) {
                return instance->GetInfo().mInstanceIdent == scheduledInstance;
            });

            if (instance != instances.end()) {
                (*instance)->SetError(AOS_ERROR_WRAP(Error(netErr, "can't prepare instance network parameters")));
            }

            mScheduledInstances.Erase(mScheduledInstances.begin() + i);
            mNetworkServiceData.Erase(mNetworkServiceData.begin() + i);
        } else {
            ++i;
        }
    }

    return ErrorEnum::eNone;
}

Error Node::SendScheduledInstances()
{
    auto stopInstances = MakeUnique<StaticArray<aos::InstanceInfo, cMaxNumInstancesPerNode>>(&mAllocator);

    for (const auto& instance : mSentInstances) {
        auto isScheduled = mScheduledInstances.ContainsIf([&instance](const aos::InstanceInfo& item) {
            return static_cast<const InstanceIdent&>(instance) == static_cast<const InstanceIdent&>(item)
                && instance.mRuntimeID == item.mRuntimeID;
        });

        if (!isScheduled) {
            if (auto err = stopInstances->EmplaceBack(); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            static_cast<InstanceIdent&>(stopInstances->Back()) = static_cast<const InstanceIdent&>(instance);
            stopInstances->Back().mRuntimeID                   = instance.mRuntimeID;
        }
    }

    if (auto err = mInstanceRunner->UpdateInstances(mInfo.mNodeID, *stopInstances, mScheduledInstances);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mSentInstances = mScheduledInstances;
    mScheduledInstances.Clear();

    return ErrorEnum::eNone;
}

RetWithError<bool> Node::ResendInstances()
{
    auto stopInstances = MakeUnique<StaticArray<aos::InstanceInfo, cMaxNumInstancesPerNode>>(&mAllocator);

    for (const auto& instance : mRunningInstances) {
        auto isRunningInstanceSent = mSentInstances.ContainsIf([&instance](const aos::InstanceInfo& item) {
            return static_cast<const InstanceIdent&>(instance) == static_cast<const InstanceIdent&>(item)
                && instance.mRuntimeID == item.mRuntimeID;
        });

        if (!isRunningInstanceSent) {
            if (auto err = stopInstances->EmplaceBack(); !err.IsNone()) {
                return {false, AOS_ERROR_WRAP(err)};
            }

            static_cast<InstanceIdent&>(stopInstances->Back()) = static_cast<const InstanceIdent&>(instance);
            stopInstances->Back().mRuntimeID                   = instance.mRuntimeID;
        }
    }

    // Instance list didn't change, skip update.
    if (stopInstances->IsEmpty() && mSentInstances.Size() == mRunningInstances.Size()) {
        return {false, ErrorEnum::eNone};
    }

    // Send request to node.
    LOG_INF() << "Resend instance update" << Log::Field("nodeID", mInfo.mNodeID);

    if (auto err = mInstanceRunner->UpdateInstances(mInfo.mNodeID, *stopInstances, mSentInstances); !err.IsNone()) {
        return {false, AOS_ERROR_WRAP(err)};
    }

    // Reset running instances to sent, in order to not duplicate requests.
    mRunningInstances.Clear();

    for (const auto& instance : mSentInstances) {
        if (auto err = mRunningInstances.EmplaceBack(); !err.IsNone()) {
            return {false, AOS_ERROR_WRAP(err)};
        }
        static_cast<InstanceIdent&>(mRunningInstances.Back()) = static_cast<const InstanceIdent&>(instance);
        mRunningInstances.Back().mRuntimeID                   = instance.mRuntimeID;
    }

    return {true, ErrorEnum::eNone};
}

bool Node::IsRunning(const InstanceIdent& id) const
{
    return mSentInstances.ContainsIf(
        [&id](const aos::InstanceInfo& info) { return static_cast<const InstanceIdent&>(info) == id; });
}

bool Node::IsScheduled(const InstanceIdent& id) const
{
    return mScheduledInstances.ContainsIf(
        [&id](const aos::InstanceInfo& info) { return static_cast<const InstanceIdent&>(info) == id; });
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

size_t Node::GetSystemCPUUsage(const monitoring::NodeMonitoringData& monitoringData) const
{
    size_t instanceUsage = 0;

    for (const auto& instance : monitoringData.mInstances) {
        instanceUsage += instance.mMonitoringData.mCPU;
    }

    if (instanceUsage > monitoringData.mMonitoringData.mCPU) {
        return 0;
    }

    return monitoringData.mMonitoringData.mCPU - instanceUsage;
}

size_t Node::GetSystemRAMUsage(const monitoring::NodeMonitoringData& monitoringData) const
{
    size_t instanceUsage = 0;

    for (const auto& instance : monitoringData.mInstances) {
        instanceUsage += instance.mMonitoringData.mRAM;
    }

    if (instanceUsage > monitoringData.mMonitoringData.mRAM) {
        return 0;
    }

    return monitoringData.mMonitoringData.mRAM - instanceUsage;
}

size_t* Node::GetPtrToAvailableCPU(const String& runtimeID)
{
    auto* runtime = mInfo.mRuntimes.FindIf(
        [&runtimeID](const RuntimeInfo& runtimeInfo) { return runtimeInfo.mRuntimeID == runtimeID; });

    if (!runtime) {
        return nullptr;
    }

    if (runtime->mAllowedDMIPS.HasValue()) {
        if (auto err = mRuntimeAvailableCPU.TryEmplace(runtimeID, runtime->mAllowedDMIPS.GetValue()); !err.IsNone()) {
            return nullptr;
        }

        return &mRuntimeAvailableCPU.Find(runtimeID)->mSecond;
    } else {
        return &mAvailableCPU;
    }
}

size_t* Node::GetPtrToAvailableRAM(const String& runtimeID)
{
    auto* runtime = mInfo.mRuntimes.FindIf(
        [&runtimeID](const RuntimeInfo& runtimeInfo) { return runtimeInfo.mRuntimeID == runtimeID; });

    if (!runtime) {
        return nullptr;
    }

    if (runtime->mAllowedRAM.HasValue()) {
        if (auto err = mRuntimeAvailableRAM.TryEmplace(runtimeID, runtime->mAllowedRAM.GetValue()); !err.IsNone()) {
            return nullptr;
        }

        return &mRuntimeAvailableRAM.Find(runtimeID)->mSecond;
    } else {
        return &mAvailableRAM;
    }
}

size_t* Node::GetPtrToMaxNumInstances(const String& runtimeID)
{
    auto* runtime = mInfo.mRuntimes.FindIf(
        [&runtimeID](const RuntimeInfo& runtimeInfo) { return runtimeInfo.mRuntimeID == runtimeID; });

    if (!runtime) {
        return nullptr;
    }

    auto maxInstances = runtime->mMaxInstances == 0 ? cMaxNumInstancesPerNode : runtime->mMaxInstances;
    if (auto err = mMaxInstances.TryEmplace(runtimeID, maxInstances); !err.IsNone()) {
        return nullptr;
    }

    return &mMaxInstances.Find(runtimeID)->mSecond;
}

} // namespace aos::cm::launcher
