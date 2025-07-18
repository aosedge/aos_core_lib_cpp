/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <limits.h>

#include <aos/cm/launcher/launcher.hpp>

#include "log.hpp"

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error NodeHandler::Init(const NodeInfo& nodeInfo, nodemanager::NodeManagerItf& nodeManager,
    resourcemanager::ResourceManagerItf& resourceManager, bool isLocalNode, bool rebalancing)
{
    LOG_DBG() << "Init node handler" << Log::Field("nodeID", nodeInfo.mNodeID);

    mInfo            = nodeInfo;
    mIsLocal         = isLocalNode;
    mIsWaiting       = true;
    mNeedRebalancing = rebalancing;
    mStatus.mInstances.Clear();

    return UpdateNodeData(nodeManager, resourceManager, rebalancing);
}

Error NodeHandler::UpdateNodeData(
    nodemanager::NodeManagerItf& nodeManager, resourcemanager::ResourceManagerItf& resourceManager, bool rebalancing)
{
    mDeviceAllocations.Clear();
    mRunRequest.mInstances.Clear();
    mRunRequest.mServices.Clear();
    mRunRequest.mLayers.Clear();
    mNeedRebalancing = rebalancing;

    if (auto err = resourceManager.GetNodeConfig(mInfo.mNodeID, mInfo.mNodeType, mConfig);
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return err;
    }

    if (auto err = ResetDeviceAllocations(); !err.IsNone()) {
        return err;
    }

    InitAvailableResources(nodeManager, rebalancing);

    return ErrorEnum::eNone;
}

void NodeHandler::SetRunStatus(const nodemanager::NodeRunInstanceStatus& status)
{
    mStatus    = status;
    mIsWaiting = false;
}

void NodeHandler::SetWaiting(bool waiting)
{
    mIsWaiting = waiting;
}

bool NodeHandler::IsWaiting() const
{
    return mIsWaiting;
}

bool NodeHandler::IsLocal() const
{
    return mIsLocal;
}

uint64_t NodeHandler::GetPartitionSize(const String& partitionType) const
{
    auto partition = mInfo.mPartitions.FindIf(
        [&partitionType](const PartitionInfo& info) { return info.mTypes.Exist(partitionType); });

    if (partition == mInfo.mPartitions.end()) {
        return 0;
    }

    return partition->mTotalSize;
}

const NodeConfig& NodeHandler::GetConfig() const
{
    return mConfig;
}

const NodeInfo& NodeHandler::GetInfo() const
{
    return mInfo;
}

const nodemanager::NodeRunInstanceStatus& NodeHandler::GetRunStatus() const
{
    return mStatus;
}

const Array<InstanceInfo> NodeHandler::GetScheduledInstances() const
{
    return mRunRequest.mInstances;
}

Error NodeHandler::StartInstances(nodemanager::NodeManagerItf& nodeManager, bool forceRestart)
{
    return nodeManager.StartInstances(
        mInfo.mNodeID, mRunRequest.mServices, mRunRequest.mLayers, mRunRequest.mInstances, forceRestart);
}

Error NodeHandler::StopInstances(nodemanager::NodeManagerItf& nodeManager, const Array<InstanceIdent>& runningInstances)
{
    return nodeManager.StopInstances(mInfo.mNodeID, runningInstances);
}

bool NodeHandler::HasDevices(const Array<oci::ServiceDevice>& devices) const
{
    for (const auto& device : devices) {
        auto allocDev = mDeviceAllocations.Find(device.mDevice);
        if (allocDev == mDeviceAllocations.end() || allocDev->mSecond == 0) {
            return false;
        }
    }

    return true;
}

Error NodeHandler::AddRunRequest(const InstanceInfo& instance, const imageprovider::ServiceInfo& serviceInfo,
    const Array<imageprovider::LayerInfo>& layers)
{
    LOG_DBG() << "Schedule instance on node" << Log::Field("instanceID", instance.mInstanceIdent)
              << Log::Field("node", mInfo.mNodeID);

    if (auto err = AllocateDevice(serviceInfo.mConfig.mDevices); !err.IsNone()) {
        return err;
    }

    auto reqCPU = GetRequestedCPU(instance.mInstanceIdent, serviceInfo.mConfig);
    if (reqCPU > mAvailableCPU && !serviceInfo.mConfig.mSkipResourceLimits) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "not enough CPU"));
    }

    auto reqRAM = GetRequestedRAM(instance.mInstanceIdent, serviceInfo.mConfig);
    if (reqRAM > mAvailableRAM && !serviceInfo.mConfig.mSkipResourceLimits) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "not enough RAM"));
    }

    if (!serviceInfo.mConfig.mSkipResourceLimits) {
        mAvailableCPU -= reqCPU;
        mAvailableRAM -= reqRAM;
    }

    if (auto err = mRunRequest.mInstances.PushBack(instance); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = AddService(serviceInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = AddLayers(layers); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Remaining resources on node" << Log::Field("nodeID", mInfo.mNodeID)
              << Log::Field("availableRAM", mAvailableRAM) << Log::Field("availableCPU", mAvailableCPU);

    return ErrorEnum::eNone;
}

Error NodeHandler::UpdateNetworkParams(const InstanceIdent& instance, const NetworkParameters& params)
{
    auto it = mRunRequest.mInstances.FindIf(
        [&instance](const InstanceInfo& info) { return info.mInstanceIdent == instance; });

    if (it == mRunRequest.mInstances.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    it->mNetworkParameters = params;

    return ErrorEnum::eNone;
}

RetWithError<StaticArray<NodeHandler*, cNodeMaxNum>> NodeHandler::GetNodesByPriorities(
    Map<StaticString<cNodeIDLen>, NodeHandler>& inNodes)
{
    StaticArray<NodeHandler*, cNodeMaxNum> nodes;

    for (auto& [_, node] : inNodes) {
        if (auto err = nodes.PushBack(&node); !err.IsNone()) {
            return {{}, AOS_ERROR_WRAP(ErrorEnum::eFailed)};
        }
    }

    nodes.Sort([](const NodeHandler* left, const NodeHandler* right) {
        if (left->GetConfig().mPriority == right->GetConfig().mPriority) {
            return left->GetInfo().mNodeID < right->GetInfo().mNodeID;
        }

        return left->GetConfig().mPriority > right->GetConfig().mPriority;
    });

    return nodes;
}

uint64_t NodeHandler::GetRequestedCPU(const InstanceIdent& instance, const oci::ServiceConfig& serviceConfig) const
{
    uint64_t requestedCPU = 0;
    auto     quota        = serviceConfig.mQuotas.mCPUDMIPSLimit;

    if (serviceConfig.mRequestedResources.HasValue() && serviceConfig.mRequestedResources->mCPU.HasValue()) {
        requestedCPU = ClampResource(*serviceConfig.mRequestedResources->mCPU, quota);
    } else {
        requestedCPU = GetReqCPUFromNodeConfig(quota, mConfig.mResourceRatios);
    }

    if (mNeedRebalancing) {
        auto cpuUsage = mAverageMonitoring.mServiceInstances.FindIf(
            [&instance](const monitoring::InstanceMonitoringData& monitoringData) {
                return monitoringData.mInstanceIdent == instance;
            });

        if (cpuUsage != mAverageMonitoring.mServiceInstances.end()) {
            if (cpuUsage->mMonitoringData.mCPU > requestedCPU) {
                return cpuUsage->mMonitoringData.mCPU;
            }
        }
    }

    return requestedCPU;
}

uint64_t NodeHandler::GetRequestedRAM(const InstanceIdent& instance, const oci::ServiceConfig& serviceConfig) const
{
    uint64_t requestedRAM = 0;
    auto     quota        = serviceConfig.mQuotas.mRAMLimit;

    if (serviceConfig.mRequestedResources.HasValue() && serviceConfig.mRequestedResources->mRAM.HasValue()) {
        requestedRAM = ClampResource(*serviceConfig.mRequestedResources->mRAM, quota);
    } else {
        requestedRAM = GetReqRAMFromNodeConfig(quota, mConfig.mResourceRatios);
    }

    if (mNeedRebalancing) {
        auto ramUsage = mAverageMonitoring.mServiceInstances.FindIf(
            [&instance](const monitoring::InstanceMonitoringData& monitoringData) {
                return monitoringData.mInstanceIdent == instance;
            });

        if (ramUsage != mAverageMonitoring.mServiceInstances.end()) {
            if (ramUsage->mMonitoringData.mRAM > requestedRAM) {
                return ramUsage->mMonitoringData.mRAM;
            }
        }
    }

    return requestedRAM;
}

uint64_t NodeHandler::GetReqStateSize(const oci::ServiceConfig& serviceConfig) const
{
    uint64_t requestedState = 0;
    auto     quota          = serviceConfig.mQuotas.mStateLimit;

    if (serviceConfig.mRequestedResources.HasValue() && serviceConfig.mRequestedResources->mState.HasValue()) {
        requestedState = ClampResource(*serviceConfig.mRequestedResources->mState, quota);
    } else {
        requestedState = GetReqStateFromNodeConfig(quota, mConfig.mResourceRatios);
    }

    return requestedState;
}

uint64_t NodeHandler::GetReqStorageSize(const oci::ServiceConfig& serviceConfig) const
{
    uint64_t requestedStorage = 0;
    auto     quota            = serviceConfig.mQuotas.mStorageLimit;

    if (serviceConfig.mRequestedResources.HasValue() && serviceConfig.mRequestedResources->mStorage.HasValue()) {
        requestedStorage = ClampResource(*serviceConfig.mRequestedResources->mStorage, quota);
    } else {
        requestedStorage = GetReqStateFromNodeConfig(quota, mConfig.mResourceRatios);
    }

    return requestedStorage;
}

uint64_t NodeHandler::GetAvailableCPU() const
{
    return mAvailableCPU;
}

uint64_t NodeHandler::GetAvailableRAM() const
{
    return mAvailableRAM;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error NodeHandler::ResetDeviceAllocations()
{
    for (const auto& device : mConfig.mDevices) {
        int sharedCount = device.mSharedCount > 0 ? device.mSharedCount : INT_MAX;

        if (auto err = mDeviceAllocations.Set(device.mName, sharedCount); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NodeHandler::AllocateDevice(const Array<oci::ServiceDevice>& devices)
{
    for (const auto& device : devices) {
        auto it = mDeviceAllocations.Find(device.mDevice);
        if (it == mDeviceAllocations.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "device not found"));
        }

        if (it->mSecond == 0) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "can't allocate device"));
        }

        it->mSecond--;
    }

    return ErrorEnum::eNone;
}

void NodeHandler::InitAvailableResources(nodemanager::NodeManagerItf& nodeManager, bool rebalancing)
{
    mAverageMonitoring.mServiceInstances.Clear();

    if (rebalancing && mConfig.mAlertRules.HasValue()) {
        const auto& alertRules = mConfig.mAlertRules.GetValue();
        if (alertRules.mCPU.HasValue() || alertRules.mRAM.HasValue()) {
            if (auto err = nodeManager.GetAverageMonitoring(mInfo.mNodeID, mAverageMonitoring); !err.IsNone()) {
                LOG_ERR() << "Can't get average monitoring" << Log::Field("nodeID", mInfo.mNodeID);
            }

            if (alertRules.mCPU.HasValue()) {
                const auto usedCPU = mAverageMonitoring.mMonitoringData.mCPU;
                const auto maxTreshold
                    = mInfo.mMaxDMIPS * static_cast<uint64_t>(alertRules.mCPU.GetValue().mMaxThreshold / 100.0);

                if (usedCPU > maxTreshold) {
                    mNeedRebalancing = true;
                }
            }

            if (alertRules.mRAM.HasValue()) {
                const auto usedRAM = mAverageMonitoring.mMonitoringData.mRAM;
                const auto maxTreshold
                    = mInfo.mMaxDMIPS * static_cast<uint64_t>(alertRules.mRAM.GetValue().mMaxThreshold / 100.0);

                if (usedRAM > maxTreshold) {
                    mNeedRebalancing = true;
                }
            }
        }
    }

    auto nodeCPU  = GetNodeCPU();
    auto nodeRAM  = GetNodeRAM();
    auto totalCPU = mInfo.mMaxDMIPS;
    auto totalRAM = mInfo.mTotalRAM;

    // For nodes required rebalancing, we need to decrease resource consumption below the low threshold.
    if (mNeedRebalancing) {
        if (mConfig.mAlertRules.HasValue()) {
            const auto& alertRules = mConfig.mAlertRules.GetValue();
            if (alertRules.mCPU.HasValue()) {
                totalCPU = static_cast<uint64_t>(mInfo.mMaxDMIPS * alertRules.mCPU.GetValue().mMinThreshold / 100.0);
            }

            if (alertRules.mRAM.HasValue()) {
                totalCPU = static_cast<uint64_t>(mInfo.mMaxDMIPS * alertRules.mRAM.GetValue().mMinThreshold / 100.0);
            }
        }
    }

    mAvailableCPU = nodeCPU > totalCPU ? 0 : totalCPU - nodeCPU;
    mAvailableRAM = nodeRAM > totalRAM ? 0 : totalRAM - nodeRAM;

    if (mNeedRebalancing) {
        LOG_DBG() << "Node resource usage" << Log::Field("nodeID", mInfo.mNodeID) << Log::Field("RAM", nodeRAM)
                  << Log::Field("CPU", nodeCPU);
    }

    LOG_DBG() << "Available resources" << Log::Field("nodeID", mInfo.mNodeID) << Log::Field("RAM", mAvailableRAM)
              << Log::Field("CPU", mAvailableCPU);
}

uint64_t NodeHandler::GetNodeCPU() const
{
    uint64_t instanceUsage = 0;
    for (const auto& instance : mAverageMonitoring.mServiceInstances) {
        instanceUsage += instance.mMonitoringData.mCPU;
    }

    if (instanceUsage > mAverageMonitoring.mMonitoringData.mCPU) {
        return 0;
    }

    return mAverageMonitoring.mMonitoringData.mCPU - instanceUsage;
}

uint64_t NodeHandler::GetNodeRAM() const
{
    uint64_t instanceUsage = 0;
    for (const auto& instance : mAverageMonitoring.mServiceInstances) {
        instanceUsage += instance.mMonitoringData.mRAM;
    }

    if (instanceUsage > mAverageMonitoring.mMonitoringData.mRAM) {
        return 0;
    }

    return mAverageMonitoring.mMonitoringData.mRAM - instanceUsage;
}

Error NodeHandler::AddService(const imageprovider::ServiceInfo& info)
{
    auto exist = mRunRequest.mServices.ExistIf(
        [&info](const ServiceInfo& item) { return item.mServiceID == info.mServiceID; });

    if (exist) {
        return ErrorEnum::eNone;
    }

    if (auto err = mRunRequest.mServices.PushBack(info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!mIsLocal) {
        mRunRequest.mServices.Back().mURL = info.mRemoteURL;
    }

    LOG_DBG() << "Schedule service on node" << Log::Field("serviceID", info.mServiceID)
              << Log::Field("node", mInfo.mNodeID);

    return ErrorEnum::eNone;
}

Error NodeHandler::AddLayers(const Array<imageprovider::LayerInfo>& layers)
{
    for (const auto& layer : layers) {
        auto exist = mRunRequest.mLayers.ExistIf(
            [&layer](const LayerInfo& item) { return item.mLayerDigest == layer.mLayerDigest; });

        if (exist) {
            continue;
        }

        if (auto err = mRunRequest.mLayers.PushBack(layer); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (!mIsLocal) {
            mRunRequest.mLayers.Back().mURL = layer.mRemoteURL;
        }

        LOG_DBG() << "Schedule layer on node" << Log::Field("digest", layer.mLayerDigest)
                  << Log::Field("node", mInfo.mNodeID);
    }

    return ErrorEnum::eNone;
}

uint64_t NodeHandler::ClampResource(uint64_t value, const Optional<uint64_t>& quota)
{
    if (quota.HasValue() && value > quota.GetValue()) {
        return quota.GetValue();
    }

    return value;
}

uint64_t NodeHandler::GetReqCPUFromNodeConfig(
    const Optional<uint64_t>& quota, const Optional<ResourceRatios>& nodeRatios)
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mCPU.HasValue()) {
        ratio = nodeRatios->mCPU.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<uint64_t>(*quota * ratio + 0.5);
    }

    return 0;
}

uint64_t NodeHandler::GetReqRAMFromNodeConfig(
    const Optional<uint64_t>& quota, const Optional<ResourceRatios>& nodeRatios)
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mRAM.HasValue()) {
        ratio = nodeRatios->mRAM.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<uint64_t>(*quota * ratio + 0.5);
    }

    return 0;
}

uint64_t NodeHandler::GetReqStateFromNodeConfig(
    const Optional<uint64_t>& quota, const Optional<ResourceRatios>& nodeRatios)
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mState.HasValue()) {
        ratio = nodeRatios->mState.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<uint64_t>(*quota * ratio + 0.5);
    }

    return 0;
}

uint64_t NodeHandler::GetReqStorageFromNodeConfig(
    const Optional<uint64_t>& quota, const Optional<ResourceRatios>& nodeRatios)
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mStorage.HasValue()) {
        ratio = nodeRatios->mStorage.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<uint64_t>(*quota * ratio + 0.5);
    }

    return 0;
}

} // namespace aos::cm::launcher
