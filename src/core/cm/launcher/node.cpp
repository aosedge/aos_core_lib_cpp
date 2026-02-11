/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "node.hpp"

namespace aos::cm::launcher {

template <typename T, class Cmp>
class Filter {
public:
    class Iterator {
    public:
        Iterator(typename Array<T>::ConstIterator it, typename Array<T>::ConstIterator end, Cmp cmp)
            : mIt(it)
            , mEnd(end)
            , mCmp(cmp)
        {
            while (mIt != mEnd && !mCmp(*mIt)) {
                ++mIt;
            }
        }

        Iterator& operator++()
        {
            assert(mIt != mEnd);

            ++mIt;

            while (mIt != mEnd && !mCmp(*mIt)) {
                ++mIt;
            }

            return *this;
        }

        Iterator operator++(int)
        {
            assert(mIt != mEnd);

            Iterator tmp = *this;

            ++(*this);

            return tmp;
        }

        bool operator==(const Iterator& other) const { return mIt == other.mIt; }
        bool operator!=(const Iterator& other) const { return mIt != other.mIt; }

        const T& operator*() const { return *mIt; }
        const T* operator->() const { return mIt; }

    private:
        typename Array<T>::ConstIterator mIt;
        typename Array<T>::ConstIterator mEnd;
        Cmp                              mCmp;
    };

    Filter(const Array<T>& array, Cmp cmp)
        : mArray(&array)
        , mCmp(cmp)
    {
    }

    Iterator begin() const { return Iterator(mArray->begin(), mArray->end(), mCmp); }
    Iterator end() const { return Iterator(mArray->end(), mArray->end(), mCmp); }

private:
    const Array<T>* mArray;
    Cmp             mCmp;
};

auto FilterActiveNodeInstances(const Array<InstanceStatus>& array, const String& nodeID)
{
    auto cmp = [nodeID](const InstanceStatus& status) {
        return status.mNodeID == nodeID && status.mState != aos::InstanceStateEnum::eInactive;
    };

    return Filter<InstanceStatus, decltype(cmp)>(array, cmp);
}

auto FilterByNode(const Array<SharedPtr<Instance>>& array, const String& nodeID)
{
    auto cmp = [nodeID](const SharedPtr<Instance>& instance) { return instance->GetInfo().mNodeID == nodeID; };

    return Filter<SharedPtr<Instance>, decltype(cmp)>(array, cmp);
}

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void Node::Init(const String& id, unitconfig::NodeConfigProviderItf& nodeConfigProvider,
    InstanceRunnerItf& instanceRunner, Allocator* allocator)
{
    mNodeConfigProvider = &nodeConfigProvider;
    mInstanceRunner     = &instanceRunner;
    mAllocator          = allocator;

    mInfo.mNodeID = id;
    mInfo.mState  = NodeStateEnum::eUnprovisioned;
}

void Node::PrepareForBalancing(bool rebalancing)
{
    UpdateConfig();

    mRuntimeAvailableCPU.Clear();
    mRuntimeAvailableRAM.Clear();
    mMaxInstances.Clear();

    mNeedBalancing = false;

    if (rebalancing && mConfig.mAlertRules.HasValue()) {
        const auto& alertRules = mConfig.mAlertRules.GetValue();
        if (alertRules.mCPU.HasValue() || alertRules.mRAM.HasValue()) {
            if (alertRules.mCPU.HasValue()) {
                const auto usedCPU = mTotalCPUUsage;
                const auto maxTreshold
                    = mInfo.mMaxDMIPS * static_cast<size_t>(alertRules.mCPU.GetValue().mMaxThreshold / 100.0);

                if (usedCPU > maxTreshold) {
                    mNeedBalancing = true;
                }
            }

            if (alertRules.mRAM.HasValue()) {
                const auto usedRAM = mTotalRAMUsage;
                const auto maxTreshold
                    = mInfo.mMaxDMIPS * static_cast<size_t>(alertRules.mRAM.GetValue().mMaxThreshold / 100.0);

                if (usedRAM > maxTreshold) {
                    mNeedBalancing = true;
                }
            }
        }
    }

    auto totalCPU = mInfo.mMaxDMIPS;
    auto totalRAM = mInfo.mTotalRAM;

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

    mAvailableCPU       = mSystemCPUUsage > totalCPU ? 0 : totalCPU - mSystemCPUUsage;
    mAvailableRAM       = mSystemRAMUsage > totalRAM ? 0 : totalRAM - mSystemRAMUsage;
    mAvailableResources = mInfo.mResources;

    if (mNeedBalancing) {
        LOG_DBG() << "Node resource usage" << Log::Field("nodeID", mInfo.mNodeID) << Log::Field("RAM", mSystemRAMUsage)
                  << Log::Field("CPU", mSystemCPUUsage);
    }

    LOG_DBG() << "Available resources" << Log::Field("nodeID", mInfo.mNodeID) << Log::Field("RAM", mAvailableRAM)
              << Log::Field("CPU", mAvailableCPU);
}

void Node::UpdateMonitoringData(const monitoring::NodeMonitoringData& monitoringData)
{
    mTotalCPUUsage = monitoringData.mMonitoringData.mCPU;
    mTotalRAMUsage = monitoringData.mMonitoringData.mRAM;

    mSystemCPUUsage = GetSystemCPUUsage(monitoringData);
    mSystemRAMUsage = GetSystemRAMUsage(monitoringData);
}

bool Node::UpdateInfo(const UnitNodeInfo& info)
{
    mInfo.mIsConnected = info.mIsConnected;

    bool nodeChanged = mInfo != info;
    if (nodeChanged) {
        mInfo = info;
    }

    return nodeChanged;
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

Error Node::ReserveResources(const InstanceIdent& instanceIdent, const String& runtimeID, size_t reqCPU, size_t reqRAM,
    const Array<StaticString<cResourceNameLen>>& reqResources)
{
    (void)instanceIdent;

    // Adjust available RAM
    auto availableRAM = GetPtrToAvailableRAM(runtimeID);
    if (availableRAM == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (*availableRAM < reqRAM) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    *availableRAM -= reqRAM;
    auto restoreRAM = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { *availableRAM += reqRAM; });

    // Adjust available CPU
    auto availableCPU = GetPtrToAvailableCPU(runtimeID);
    if (availableCPU == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (*availableCPU < reqCPU) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    *availableCPU -= reqCPU;
    auto restoreCPU = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { *availableCPU += reqCPU; });

    // Adjust max number of instances
    auto maxNumInstances = GetPtrToMaxNumInstances(runtimeID);
    if (maxNumInstances == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (*maxNumInstances == 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    (*maxNumInstances)--;
    auto restoreMaxNumInstances = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { (*maxNumInstances)++; });

    // Adjust shared resources
    auto curIt            = reqResources.begin();
    auto restoreResources = DeferRelease(reinterpret_cast<int*>(1), [&](int*) {
        for (auto restoreIt = reqResources.begin(); restoreIt != curIt; ++restoreIt) {
            auto availableResource = mAvailableResources.FindIf(
                [restoreIt](const ResourceInfo& info) { return info.mName == *restoreIt; });
            assert(availableResource != mAvailableResources.end());

            availableResource->mSharedCount++;
        }
    });

    for (; curIt != reqResources.end(); ++curIt) {
        auto availableResource
            = mAvailableResources.FindIf([curIt](const ResourceInfo& info) { return info.mName == *curIt; });

        if (availableResource == mAvailableResources.end() || availableResource->mSharedCount < 1) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        availableResource->mSharedCount--;
    }

    restoreResources.Release();
    restoreMaxNumInstances.Release();
    restoreCPU.Release();
    restoreRAM.Release();

    return ErrorEnum::eNone;
}

Error Node::SendScheduledInstances(
    const Array<SharedPtr<Instance>>& scheduledInstances, const Array<InstanceStatus>& runningInstances)
{
    auto stopInstances  = MakeUnique<StaticArray<aos::InstanceInfo, cMaxNumInstances>>(mAllocator);
    auto startInstances = MakeUnique<StaticArray<aos::InstanceInfo, cMaxNumInstances>>(mAllocator);

    for (const auto& status : FilterActiveNodeInstances(runningInstances, mInfo.mNodeID)) {
        // Check if the instance is scheduled on this node.
        auto isScheduled = scheduledInstances.ContainsIf([&status, this](const SharedPtr<Instance>& item) {
            return static_cast<const InstanceIdent&>(status) == item->GetInfo().mInstanceIdent
                && status.mRuntimeID == item->GetInfo().mRuntimeID && item->GetInfo().mNodeID == mInfo.mNodeID;
        });

        if (!isScheduled) {
            if (auto err = stopInstances->EmplaceBack(); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            Convert(status, stopInstances->Back());
        }
    }

    for (const auto& instance : FilterByNode(scheduledInstances, mInfo.mNodeID)) {
        if (auto err = startInstances->PushBack(instance->GetSMInfo()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    LOG_INF() << "Update node instances" << Log::Field("nodeID", mInfo.mNodeID)
              << Log::Field("stopInstances", stopInstances->Size())
              << Log::Field("startInstances", startInstances->Size());

    for (const auto& instance : *stopInstances) {
        LOG_INF() << "Update node stop instance" << Log::Field("instance", static_cast<const InstanceIdent&>(instance))
                  << Log::Field("version", instance.mVersion) << Log::Field("runtimeID", instance.mRuntimeID);
    }

    for (const auto& instance : *startInstances) {
        LOG_INF() << "Update node start instance" << Log::Field("instance", static_cast<const InstanceIdent&>(instance))
                  << Log::Field("version", instance.mVersion) << Log::Field("runtimeID", instance.mRuntimeID);
    }

    if (auto err = mInstanceRunner->UpdateInstances(mInfo.mNodeID, *stopInstances, *startInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<bool> Node::ResendInstances(
    const Array<SharedPtr<Instance>>& activeInstances, const Array<InstanceStatus>& runningInstances)
{
    auto   stopInstances        = MakeUnique<StaticArray<aos::InstanceInfo, cMaxNumInstances>>(mAllocator);
    auto   startInstances       = MakeUnique<StaticArray<aos::InstanceInfo, cMaxNumInstances>>(mAllocator);
    size_t runningNodeInstances = 0;

    for (const auto& status : FilterActiveNodeInstances(runningInstances, mInfo.mNodeID)) {
        runningNodeInstances++;

        auto isActive = activeInstances.ContainsIf([&status](const SharedPtr<Instance>& item) {
            return static_cast<const InstanceIdent&>(status) == item->GetInfo().mInstanceIdent
                && status.mRuntimeID == item->GetInfo().mRuntimeID;
        });

        if (!isActive) {
            if (auto err = stopInstances->EmplaceBack(); !err.IsNone()) {
                return {false, AOS_ERROR_WRAP(err)};
            }

            Convert(status, stopInstances->Back());
        }
    }

    for (const auto& instance : FilterByNode(activeInstances, mInfo.mNodeID)) {
        if (auto err = startInstances->PushBack(instance->GetSMInfo()); !err.IsNone()) {
            return {false, AOS_ERROR_WRAP(err)};
        }
    }

    // Instance list didn't change, skip update.
    if (stopInstances->IsEmpty() && startInstances->Size() == runningNodeInstances) {
        return {false, ErrorEnum::eNone};
    }

    // Send request to node.
    LOG_INF() << "Resend instance update" << Log::Field("nodeID", mInfo.mNodeID)
              << Log::Field("stopInstances", stopInstances->Size())
              << Log::Field("startInstances", startInstances->Size());

    for (const auto& instance : *stopInstances) {
        LOG_INF() << "Update node stop instance" << Log::Field("instance", static_cast<const InstanceIdent&>(instance))
                  << Log::Field("version", instance.mVersion) << Log::Field("runtimeID", instance.mRuntimeID);
    }

    for (const auto& instance : *startInstances) {
        LOG_INF() << "Update node start instance" << Log::Field("instance", static_cast<const InstanceIdent&>(instance))
                  << Log::Field("version", instance.mVersion) << Log::Field("runtimeID", instance.mRuntimeID);
    }

    if (auto err = mInstanceRunner->UpdateInstances(mInfo.mNodeID, *stopInstances, *startInstances); !err.IsNone()) {
        return {false, AOS_ERROR_WRAP(err)};
    }

    return {true, ErrorEnum::eNone};
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

    auto maxInstances = runtime->mMaxInstances == 0 ? cMaxNumInstances : runtime->mMaxInstances;
    if (auto err = mMaxInstances.TryEmplace(runtimeID, maxInstances); !err.IsNone()) {
        return nullptr;
    }

    return &mMaxInstances.Find(runtimeID)->mSecond;
}

void Node::Convert(const InstanceStatus& status, aos::InstanceInfo& info)
{
    static_cast<InstanceIdent&>(info) = static_cast<const InstanceIdent&>(status);
    info.mRuntimeID                   = status.mRuntimeID;
}

} // namespace aos::cm::launcher
