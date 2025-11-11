/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "balancer.hpp"

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void Balancer::Init(InstanceManager& instanceManager, ImageInfoProviderItf& imageInfoProvider, NodeManager& nodeManager,
    MonitoringProviderItf& monitorProvider, InstanceRunnerItf& runner,
    networkmanager::NetworkManagerItf& networkManager)
{
    mInstanceManager   = &instanceManager;
    mImageInfoProvider = &imageInfoProvider;
    mNodeManager       = &nodeManager;
    mMonitorProvider   = &monitorProvider;
    mRunner            = &runner;
    mNetworkManager    = &networkManager;
}

Error Balancer::RunInstances(const Array<RunInstanceRequest>& instances, bool rebalancing)
{
    auto sortedInstances = MakeUnique<StaticArray<RunInstanceRequest, cMaxNumInstances>>(&mAllocator);
    *sortedInstances     = instances;

    sortedInstances->Sort([](const RunInstanceRequest& left, const RunInstanceRequest& right) {
        return left.mPriority > right.mPriority || (left.mPriority == right.mPriority && left.mItemID < right.mItemID);
    });

    if (rebalancing) {
        if (auto err = PerformPolicyBalancing(*sortedInstances); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = PerformNodeBalancing(*sortedInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = UpdateNetwork(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Error firstErr = mInstanceManager->SubmitStash();

    for (auto& node : mNodeManager->GetNodes()) {
        if (auto err = node.SendUpdate(); !err.IsNone()) {
            LOG_ERR() << "Can't send instance update" << Log::Field("nodeID", node.GetInfo().mNodeID)
                      << Log::Field(err);

            if (firstErr.IsNone()) {
                firstErr = err;
            }
        }
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Balancer::SetupInstanceInfo(const oci::ServiceConfig& servConf, const NodeConfig& nodeConf,
    const RunInstanceRequest& request, const String& imageID, const String& runtimeID, const Instance& instance,
    aos::InstanceInfo& info)
{
    // create instance info, InstanceNetworkParameters are added after network updates
    static_cast<InstanceIdent&>(info) = instance.GetInfo().mInstanceIdent;
    info.mImageID                     = imageID;
    info.mRuntimeID                   = runtimeID;
    info.mPriority                    = request.mPriority;
    info.mUID                         = instance.GetInfo().mUID;

    auto [gid, getGIDErr] = mImageInfoProvider->GetServiceGID(info.mItemID);
    if (!getGIDErr.IsNone() && !getGIDErr.Is(ErrorEnum::eNotSupported)) {
        return AOS_ERROR_WRAP(getGIDErr);
    }

    if (auto err = mNodeManager->SetupStateStorage(nodeConf, servConf, gid, info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Balancer::PerformNodeBalancing(const Array<RunInstanceRequest>& requests)
{
    for (const auto& request : requests) {
        for (size_t i = 0; i < request.mNumInstances; i++) {
            InstanceIdent instanceIdent {request.mItemID, request.mSubjectID, i};
            if (mNodeManager->IsScheduled(instanceIdent)) {
                continue;
            }

            if (auto err = mInstanceManager->AddInstanceToStash(instanceIdent, request); !err.IsNone()) {
                LOG_ERR() << "Can't create new instance" << Log::Field("instance", instanceIdent.mItemID)
                          << Log::Field(err);

                break;
            }

            auto instance = mInstanceManager->FindStashInstance(instanceIdent);
            auto images   = MakeUnique<StaticArray<ImageInfo, cMaxNumUpdateImages>>(&mAllocator);

            if (auto err = (*instance)->GetItemImages(*images); !err.IsNone()) {
                LOG_ERR() << "Can't get images" << Log::Field("instance", instanceIdent.mItemID) << Log::Field(err);

                return err;
            }

            bool  isInstanceScheduled = false;
            Error scheduleErr         = ErrorEnum::eNone;

            for (const auto& image : *images) {
                if (auto err = ScheduleInstance(**instance, request, image.mImageID); err.IsNone()) {
                    isInstanceScheduled = true;

                    break;
                } else {
                    scheduleErr = err;
                }
            }

            if (!isInstanceScheduled) {
                (*instance)->SetError(scheduleErr);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error Balancer::ScheduleInstance(Instance& instance, const RunInstanceRequest& request, const String& imageID)
{
    auto nodes         = MakeUnique<StaticArray<Node*, cMaxNumNodes>>(&mAllocator);
    auto serviceConfig = MakeUnique<oci::ServiceConfig>(&mAllocator);
    auto imageConfig   = MakeUnique<oci::ImageConfig>(&mAllocator);

    // get configs
    if (auto err = instance.GetServiceConfig(imageID, *serviceConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = instance.GetImageConfig(imageID, *imageConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // select node & runtime
    if (auto err = mNodeManager->GetNodesByPriorities(*nodes); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = FilterNodesByStaticResources(*serviceConfig, request, *nodes); !err.IsNone()) {
        return err;
    }

    auto [nodeRuntime, selectErr] = SelectRuntimeForInstance(instance, *serviceConfig, *nodes);
    if (!selectErr.IsNone()) {
        return selectErr;
    }

    auto&       node    = nodeRuntime.mFirst;
    const auto& runtime = nodeRuntime.mSecond;

    // create network params
    auto networkServiceData = MakeUnique<networkmanager::NetworkServiceData>(&mAllocator);

    networkServiceData->mExposedPorts       = imageConfig->mExposedPorts;
    networkServiceData->mAllowedConnections = serviceConfig->mAllowedConnections;
    if (serviceConfig->mHostname.HasValue()) {
        networkServiceData->mHosts.PushBack(serviceConfig->mHostname.GetValue());
    }

    // create instance info, InstanceNetworkParameters will be added after network update
    auto instanceInfo = MakeUnique<aos::InstanceInfo>(&mAllocator);

    if (auto err = SetupInstanceInfo(
            *serviceConfig, node->GetConfig(), request, imageID, runtime->mRuntimeID, instance, *instanceInfo);
        !err.IsNone()) {
        return err;
    }

    // schedule instance
    auto reqCPU       = GetRequestedCPU(instance, *node, *serviceConfig);
    auto reqRAM       = GetRequestedRAM(instance, *node, *serviceConfig);
    auto reqResources = serviceConfig->mResources;

    if (auto err
        = node->ScheduleInstance(*instanceInfo, request.mProviderID, *networkServiceData, reqCPU, reqRAM, reqResources);
        !err.IsNone()) {
        return err;
    }

    if (auto err = instance.Schedule(*instanceInfo, node->GetInfo().mNodeID); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error Balancer::FilterNodesByStaticResources(
    const oci::ServiceConfig& serviceConfig, const RunInstanceRequest& request, Array<Node*>& nodes)
{
    FilterNodesByLabels(request.mLabels, nodes);
    if (nodes.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no nodes with instance labels"));
    }

    FilterNodesByResources(serviceConfig.mResources, nodes);
    if (nodes.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no nodes with with service resources"));
    }

    return ErrorEnum::eNone;
}

void Balancer::FilterNodesByLabels(const Array<StaticString<cLabelNameLen>>& labels, Array<Node*>& nodes)
{
    if (labels.IsEmpty()) {
        return;
    }

    nodes.RemoveIf([&labels](const Node* node) {
        for (const auto& label : labels) {
            if (!node->GetConfig().mLabels.Contains(label)) {
                return true;
            }
        }

        return false;
    });
}

void Balancer::FilterNodesByResources(const Array<StaticString<cResourceNameLen>>& resources, Array<Node*>& nodes)
{
    if (resources.IsEmpty()) {
        return;
    }

    nodes.RemoveIf([&resources](const Node* node) {
        for (const auto& resource : resources) {
            auto matchResource
                = [&resource](const ResourceInfo& info) { return info.mName == resource && info.mSharedCount > 0; };

            if (!node->GetInfo().mResources.ContainsIf(matchResource)) {
                return true;
            }
        }

        return false;
    });
}

RetWithError<Pair<Node*, const RuntimeInfo*>> Balancer::SelectRuntimeForInstance(
    Instance& instance, const oci::ServiceConfig& config, Array<Node*>& nodes)
{
    auto nodeRuntimes
        = MakeUnique<StaticMap<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>, cMaxNumInstances>>(
            &mAllocator);

    if (auto err = SelectRuntimes(config.mRunners, nodes, *nodeRuntimes); !err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    FilterByCPU(instance, config, *nodeRuntimes);
    if (nodeRuntimes->IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no runtimes with requested CPU"))};
    }

    FilterByRAM(instance, config, *nodeRuntimes);
    if (nodeRuntimes->IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no runtimes with requested RAM"))};
    }

    FilterByNumInstances(*nodeRuntimes);
    if (nodeRuntimes->IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no runtimes with requested RAM"))};
    }

    FilterTopPriorityNodes(*nodeRuntimes);
    if (nodeRuntimes->IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "failed top priority nodes filtering"))};
    }

    // Select node with the most resources.
    nodes.RemoveIf([&nodeRuntimes](Node* node) { return !nodeRuntimes->Contains(node); });

    nodes.Sort([](Node* left, Node* right) {
        if (left->GetAvailableCPU() != right->GetAvailableCPU()) {
            return left->GetAvailableCPU() > right->GetAvailableCPU();
        }

        return left->GetAvailableRAM() > right->GetAvailableRAM();
    });

    const auto& bestNode         = nodes.Front();
    auto&       bestNodeRuntimes = nodeRuntimes->Find(bestNode)->mSecond;

    bestNodeRuntimes.Sort(
        [](const RuntimeInfo* left, const RuntimeInfo* right) { return left->mRuntimeType < right->mRuntimeType; });

    Pair<Node*, const RuntimeInfo*> result {bestNode, bestNodeRuntimes.Front()};

    return {result, ErrorEnum::eNone};
}

Error Balancer::SelectRuntimes(const Array<StaticString<cRunnerNameLen>>& inRunners, Array<Node*>& nodes,
    Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& runtimes)
{
    nodes.RemoveIf([&inRunners, this](const Node* node) {
        for (const auto& runtime : node->GetInfo().mRuntimes) {
            if (inRunners.Contains(runtime.mRuntimeID)) {
                return false;
            }
        }

        return true;
    });

    runtimes.Clear();

    for (auto node : nodes) {
        if (auto err = runtimes.Emplace(node); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& suitableNodeRuntimes = runtimes.Find(node)->mSecond;

        for (const auto& runner : inRunners) {
            auto nodeRuntime = node->GetInfo().mRuntimes.FindIf(
                [&runner](const RuntimeInfo& runtime) { return runtime.mRuntimeID == runner; });

            if (nodeRuntime) {
                suitableNodeRuntimes.PushBack(nodeRuntime);
            }
        }

        if (suitableNodeRuntimes.IsEmpty()) {
            runtimes.Remove(node);
        }
    }

    if (runtimes.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no runtimes of specified type"));
    }

    return ErrorEnum::eNone;
}

void Balancer::FilterByCPU(Instance& instance, const oci::ServiceConfig& serviceConfig,
    Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& nodes)
{
    for (auto it = nodes.begin(); it != nodes.end();) {
        auto& node         = it->mFirst;
        auto& nodeRuntimes = it->mSecond;
        auto  reqCPU       = GetRequestedCPU(instance, *it->mFirst, serviceConfig);

        LOG_DBG() << "Requested CPU" << Log::Field("nodeID", node->GetInfo().mNodeID) << Log::Field("CPU", reqCPU);

        for (auto runtimeIt = nodeRuntimes.begin(); runtimeIt != nodeRuntimes.end();) {
            auto availCPU = node->GetAvailableCPU((*runtimeIt)->mRuntimeID);

            if (availCPU < reqCPU) {
                runtimeIt = nodeRuntimes.Erase(runtimeIt);
            } else {
                runtimeIt++;
            }
        }

        if (nodeRuntimes.IsEmpty()) {
            it = nodes.Erase(it);
        } else {
            it++;
        }
    }
}

void Balancer::FilterByRAM(Instance& instance, const oci::ServiceConfig& serviceConfig,
    Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& nodes)
{
    for (auto it = nodes.begin(); it != nodes.end();) {
        auto& node         = it->mFirst;
        auto& nodeRuntimes = it->mSecond;
        auto  reqRAM       = GetRequestedRAM(instance, *it->mFirst, serviceConfig);

        LOG_DBG() << "Requested RAM" << Log::Field("nodeID", node->GetInfo().mNodeID) << Log::Field("RAM", reqRAM);

        for (auto runtimeIt = nodeRuntimes.begin(); runtimeIt != nodeRuntimes.end();) {
            auto availRAM = node->GetAvailableRAM((*runtimeIt)->mRuntimeID);

            if (availRAM < reqRAM) {
                runtimeIt = nodeRuntimes.Erase(runtimeIt);
            } else {
                runtimeIt++;
            }
        }

        if (nodeRuntimes.IsEmpty()) {
            it = nodes.Erase(it);
        } else {
            it++;
        }
    }
}

void Balancer::FilterByNumInstances(Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& nodes)
{
    for (auto it = nodes.begin(); it != nodes.end();) {
        auto& node         = it->mFirst;
        auto& nodeRuntimes = it->mSecond;

        for (auto runtimeIt = nodeRuntimes.begin(); runtimeIt != nodeRuntimes.end();) {
            if (node->IsMaxNumInstancesReached((*runtimeIt)->mRuntimeID)) {
                LOG_DBG() << "Max instances reached for runtime" << Log::Field("nodeID", node->GetInfo().mNodeID)
                          << Log::Field("runtimeID", (*runtimeIt)->mRuntimeID);
                runtimeIt = nodeRuntimes.Erase(runtimeIt);
            } else {
                runtimeIt++;
            }
        }

        if (nodeRuntimes.IsEmpty()) {
            it = nodes.Erase(it);
        } else {
            it++;
        }
    }
}

void Balancer::FilterTopPriorityNodes(Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& nodes)
{
    if (nodes.IsEmpty()) {
        return;
    }

    using NodeRuntimes = Pair<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>;

    // cppcheck-suppress templateRecursion
    auto topPriorityNode = nodes.Min([](const NodeRuntimes& left, const NodeRuntimes& right) {
        return left.mFirst->GetConfig().mPriority > right.mFirst->GetConfig().mPriority;
    });

    auto topPriority = topPriorityNode->mFirst->GetConfig().mPriority;

    nodes.RemoveIf(
        [topPriority](const NodeRuntimes& item) { return item.mFirst->GetConfig().mPriority != topPriority; });
}

size_t Balancer::GetRequestedCPU(Instance& instance, const Node& node, const oci::ServiceConfig& serviceConfig)
{
    auto reqCPU = instance.GetRequestedCPU(node.GetConfig(), serviceConfig);

    if (node.NeedBalancing()) {
        if (instance.GetMonitoringData().mCPU > reqCPU) {
            return instance.GetMonitoringData().mCPU;
        }
    }

    return reqCPU;
}

size_t Balancer::GetRequestedRAM(Instance& instance, const Node& node, const oci::ServiceConfig& serviceConfig)
{
    auto reqRAM = instance.GetRequestedRAM(node.GetConfig(), serviceConfig);

    if (node.NeedBalancing()) {
        if (instance.GetMonitoringData().mRAM > reqRAM) {
            return instance.GetMonitoringData().mRAM;
        }
    }

    return reqRAM;
}

Error Balancer::UpdateNetwork()
{
    if (auto err = RemoveNetworkForDeletedInstances(); !err.IsNone()) {
        return err;
    }

    if (auto err = SetupNetworkForNewInstances(); !err.IsNone()) {
        return err;
    }

    if (auto err = SetNetworkParams(true); !err.IsNone()) {
        return err;
    }

    if (auto err = SetNetworkParams(false); !err.IsNone()) {
        return err;
    }

    if (auto err = mNetworkManager->RestartDNSServer(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Balancer::RemoveNetworkForDeletedInstances()
{
    const auto& stashInstances = mInstanceManager->GetStashInstances();

    // remove network for deleted instances
    for (const auto& instance : mInstanceManager->GetActiveInstances()) {
        bool isScheduled = stashInstances.ContainsIf(
            [&instance](const SharedPtr<Instance>& stashInst) { return stashInst.Get() == instance.Get(); });

        if (!isScheduled) {
            const auto& info = instance->GetInfo();

            if (auto err = mNetworkManager->RemoveInstanceNetworkParameters(info.mInstanceIdent, info.mNodeID);
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error Balancer::SetNetworkParams(bool onlyWithExposedPorts)
{
    // update network for new instance list
    for (auto& node : mNodeManager->GetNodes()) {
        if (auto err = node.SetupNetworkParams(onlyWithExposedPorts, *mNetworkManager); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Balancer::SetupNetworkForNewInstances()
{
    // update network for new instance list
    for (const auto& node : mNodeManager->GetNodes()) {
        const auto& nodeID = node.GetInfo().mNodeID;

        auto providers = MakeUnique<StaticArray<StaticString<cIDLen>, cMaxNumInstances>>(&mAllocator);

        for (const auto& instance : mInstanceManager->GetStashInstances()) {
            if (nodeID == instance->GetInfo().mNodeID) {
                if (auto err = providers->PushBack(instance->GetProviderID()); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }
            }
        }

        if (auto err = mNetworkManager->UpdateProviderNetwork(*providers, nodeID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Balancer::PerformPolicyBalancing(const Array<RunInstanceRequest>& requests)
{
    for (const auto& request : requests) {
        auto images        = MakeUnique<StaticArray<ImageInfo, cMaxNumUpdateImages>>(&mAllocator);
        auto serviceConfig = MakeUnique<oci::ServiceConfig>(&mAllocator);
        auto imageConfig   = MakeUnique<oci::ImageConfig>(&mAllocator);

        for (size_t i = 0; i < request.mNumInstances; i++) {
            InstanceIdent instanceIdent {request.mItemID, request.mSubjectID, i};

            if (!mNodeManager->IsRunning(instanceIdent)) {
                continue;
            }

            auto instance = mInstanceManager->FindActiveInstance(instanceIdent);
            if (!instance) {
                return AOS_ERROR_WRAP(ErrorEnum::eFailed);
            }

            auto imageID = (*instance)->GetInfo().mImageID;

            // get configs
            if (auto err = mImageInfoProvider->GetServiceConfig(instanceIdent.mItemID, imageID, *serviceConfig);
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            if (serviceConfig->mBalancingPolicy != oci::BalancingPolicyEnum::eBalancingDisabled) {
                continue;
            }

            if (auto err = mImageInfoProvider->GetImageConfig(instanceIdent.mItemID, imageID, *imageConfig);
                !err.IsNone()) {
                (*instance)->SetError(err);

                continue;
            }

            // stash instance
            if (auto err = mInstanceManager->AddInstanceToStash(instanceIdent, request); !err.IsNone()) {
                LOG_ERR() << "Can't create new instance" << Log::Field("instance", instanceIdent.mItemID)
                          << Log::Field(err);
                (*instance)->SetError(err);

                continue;
            }

            // create network params
            auto networkServiceData = MakeUnique<networkmanager::NetworkServiceData>(&mAllocator);

            networkServiceData->mExposedPorts       = imageConfig->mExposedPorts;
            networkServiceData->mAllowedConnections = serviceConfig->mAllowedConnections;
            if (serviceConfig->mHostname.HasValue()) {
                networkServiceData->mHosts.PushBack(serviceConfig->mHostname.GetValue());
            }

            // find node
            auto node = mNodeManager->FindNode((*instance)->GetInfo().mNodeID);
            if (!node) {
                (*instance)->SetError(AOS_ERROR_WRAP(ErrorEnum::eFailed));

                continue;
            }

            // create instance info, InstanceNetworkParameters will be added after network update
            auto instanceInfo = MakeUnique<aos::InstanceInfo>(&mAllocator);

            if (auto err = SetupInstanceInfo(*serviceConfig, node->GetConfig(), request, imageID,
                    (*instance)->GetInfo().mRuntimeID, **instance, *instanceInfo);
                !err.IsNone()) {
                (*instance)->SetError(err);

                continue;
            }

            // calculate requested resources
            auto reqCPU       = GetRequestedCPU(**instance, *node, *serviceConfig);
            auto reqRAM       = GetRequestedRAM(**instance, *node, *serviceConfig);
            auto reqResources = serviceConfig->mResources;

            // schedule instance
            if (auto err = node->ScheduleInstance(
                    *instanceInfo, request.mProviderID, *networkServiceData, reqCPU, reqRAM, reqResources);
                !err.IsNone()) {
                (*instance)->SetError(err);

                continue;
            }

            if (auto err = (*instance)->Schedule(*instanceInfo, node->GetInfo().mNodeID); !err.IsNone()) {
                (*instance)->SetError(err);

                continue;
            }
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
