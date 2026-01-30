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

void Balancer::Init(InstanceManager& instanceManager, imagemanager::ItemInfoProviderItf& itemInfoProvider,
    oci::OCISpecItf& ociSpec, NodeManager& nodeManager, MonitoringProviderItf& monitorProvider,
    InstanceRunnerItf& runner, NetworkManager& networkManager)
{
    mInstanceManager = &instanceManager;
    mImageInfoProvider.Init(itemInfoProvider, ociSpec);
    mNodeManager     = &nodeManager;
    mMonitorProvider = &monitorProvider;
    mRunner          = &runner;
    mNetworkManager  = &networkManager;
}

Error Balancer::RunInstances(UniqueLock<Mutex>& lock, bool rebalancing)
{
    if (auto err = PrepareForBalancing(rebalancing); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (rebalancing) {
        if (auto err = PerformPolicyBalancing(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = PerformNodeBalancing(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = UpdateNetwork(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInstanceManager->SubmitScheduledInstances(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNodeManager->SendScheduledInstances(lock); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Balancer::LoadInstances()
{
    PrepareForBalancing(false);

    auto err = mNodeManager->LoadInstances(mInstanceManager->GetActiveInstances(), mImageInfoProvider);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Balancer::PerformNodeBalancing()
{
    LOG_DBG() << "Perform node balancing" << Log::Field("numNodes", mNodeManager->GetNodes().Size())
              << Log::Field("numInstances", mInstanceManager->GetScheduledInstances().Size());

    for (const auto& instance : mInstanceManager->GetScheduledInstances()) {
        const auto& info = instance->GetInfo();
        const auto& id   = info.mInstanceIdent;

        LOG_DBG() << "Perform node balancing" << Log::Field("instance", id);

        if (mNodeManager->IsScheduled(id)) {
            LOG_DBG() << "Instance aready scheduled" << Log::Field("instance", id);

            continue;
        }

        auto imageIndex = MakeUnique<oci::ImageIndex>(&mAllocator);

        if (auto err = mImageInfoProvider.GetImageIndex(id.mItemID, info.mVersion, *imageIndex); !err.IsNone()) {
            LOG_ERR() << "Can't get images" << Log::Field("instance", id) << Log::Field(err);

            continue;
        }

        Error scheduleErr = ErrorEnum::eNone;

        for (const auto& manifest : imageIndex->mManifests) {
            LOG_DBG() << "Try to schedule instance" << Log::Field("instance", id)
                      << Log::Field("manifest", manifest.mDigest);

            if (auto err = ScheduleInstance(*instance, manifest); err.IsNone()) {
                LOG_DBG() << "Instance scheduled successfully" << Log::Field("nodeID", info.mNodeID);

                break;
            } else {
                LOG_ERR() << "Can't schedule instance" << Log::Field(err);

                scheduleErr = err;
            }
        }

        if (!scheduleErr.IsNone()) {
            instance->SetError(scheduleErr);
        }
    }

    return ErrorEnum::eNone;
}

Error Balancer::ScheduleInstance(Instance& instance, const oci::IndexContentDescriptor& imageDescriptor)
{
    auto nodes        = MakeUnique<StaticArray<Node*, cMaxNumNodes>>(&mAllocator);
    auto instanceInfo = MakeUnique<aos::InstanceInfo>(&mAllocator);

    auto releaseConfigs = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { instance.ResetConfigs(); });

    if (auto err = instance.LoadConfigs(imageDescriptor); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "can't load instance configs"));
    }

    // Select node runtimes
    if (auto err = mNodeManager->GetConnectedNodes(*nodes); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "get connected nodes failed"));
    }

    if (auto err = SelectNodes(instance, *nodes); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "can't find node for instance"));
    }

    auto [nodeRuntime, selectErr] = SelectRuntime(instance, *nodes);
    if (!selectErr.IsNone()) {
        return AOS_ERROR_WRAP(Error(selectErr, "can't find runtime for instance"));
    }

    // Schedule instance
    auto&       node    = nodeRuntime.mFirst;
    const auto& runtime = nodeRuntime.mSecond;

    if (auto err = instance.Schedule(*node, runtime->mRuntimeID, *instanceInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "can't schedule instance"));
    }

    return ErrorEnum::eNone;
}

Error Balancer::SelectNodes(Instance& instance, Array<Node*>& nodes)
{
    FilterNodesByLabels(instance, nodes);
    if (nodes.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no nodes with instance labels"));
    }

    FilterNodesByResources(instance, nodes);
    if (nodes.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no nodes with with service resources"));
    }

    return ErrorEnum::eNone;
}

void Balancer::FilterNodesByLabels(Instance& instance, Array<Node*>& nodes)
{
    nodes.RemoveIf([&instance](const Node* node) { return !instance.AreNodeLabelsOk(node->GetConfig().mLabels); });
}

void Balancer::FilterNodesByResources(Instance& instance, Array<Node*>& nodes)
{
    nodes.RemoveIf([&instance](const Node* node) { return !instance.AreNodeResourcesOk(node->GetInfo().mResources); });
}

RetWithError<Pair<Node*, const RuntimeInfo*>> Balancer::SelectRuntime(Instance& instance, Array<Node*>& nodes)
{
    auto nodeRuntimes = MakeUnique<NodeRuntimes>(&mAllocator);

    if (auto err = CreateRuntimes(nodes, *nodeRuntimes); !err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    FilterByRuntimeType(instance, *nodeRuntimes);
    if (nodeRuntimes->IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no runtimes with requested runtime type"))};
    }

    FilterByPlatform(instance, *nodeRuntimes);
    if (nodeRuntimes->IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no runtimes with requested platform"))};
    }

    FilterByCPU(instance, *nodeRuntimes);
    if (nodeRuntimes->IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no runtimes with requested CPU"))};
    }

    FilterByRAM(instance, *nodeRuntimes);
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

    // Select best node.
    using NodeRuntimesItem = NodeRuntimes::ValueType;

    auto nodeCmp = [](const NodeRuntimesItem& left, const NodeRuntimesItem& right) {
        if (left.mFirst->GetAvailableCPU() != right.mFirst->GetAvailableCPU()) {
            return left.mFirst->GetAvailableCPU() > right.mFirst->GetAvailableCPU();
        }

        return left.mFirst->GetAvailableRAM() > right.mFirst->GetAvailableRAM();
    };

    auto& bestNode = *nodeRuntimes->Min(nodeCmp);

    // Select best runtime.
    auto& bestNodeRuntimes = bestNode.mSecond;

    bestNodeRuntimes.Sort(
        [](const RuntimeInfo* left, const RuntimeInfo* right) { return left->mRuntimeType < right->mRuntimeType; });

    // Return result.
    Pair<Node*, const RuntimeInfo*> result {bestNode.mFirst, bestNodeRuntimes.Front()};

    return {result, ErrorEnum::eNone};
}

Error Balancer::CreateRuntimes(Array<Node*>& nodes, NodeRuntimes& runtimes)
{
    for (auto node : nodes) {
        if (auto err = runtimes.Emplace(node); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& nodeRuntimes = runtimes.Find(node)->mSecond;

        for (const auto& nodeRuntime : node->GetInfo().mRuntimes) {
            if (auto err = nodeRuntimes.PushBack(&nodeRuntime); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        // Remove node with no runtimes
        if (nodeRuntimes.IsEmpty()) {
            runtimes.Remove(node);
        }
    }

    return ErrorEnum::eNone;
}

template <typename Filter>
void Balancer::FilterRuntimes(NodeRuntimes& runtimes, Filter& filter)
{
    for (auto it = runtimes.begin(); it != runtimes.end();) {
        auto& node         = it->mFirst;
        auto& nodeRuntimes = it->mSecond;

        for (auto runtimeIt = nodeRuntimes.begin(); runtimeIt != nodeRuntimes.end();) {
            if (!filter(node, *runtimeIt)) {
                runtimeIt = nodeRuntimes.Erase(runtimeIt);
            } else {
                runtimeIt++;
            }
        }

        if (nodeRuntimes.IsEmpty()) {
            it = runtimes.Erase(it);
        } else {
            it++;
        }
    }
}

void Balancer::FilterByRuntimeType(Instance& instance, NodeRuntimes& runtimes)
{
    auto filter = [&instance](Node* node, const RuntimeInfo* runtime) {
        (void)node;

        return instance.IsRuntimeTypeOk(runtime->mRuntimeType);
    };

    FilterRuntimes(runtimes, filter);
}

void Balancer::FilterByPlatform(Instance& instance, NodeRuntimes& runtimes)
{
    auto filter = [&instance](Node* node, const RuntimeInfo* runtime) {
        (void)node;

        return instance.IsPlatformOk(*runtime);
    };

    FilterRuntimes(runtimes, filter);
}

void Balancer::FilterByCPU(Instance& instance, NodeRuntimes& nodes)
{
    auto filter = [&instance](Node* node, const RuntimeInfo* runtime) {
        auto availCPU = node->GetAvailableCPU(runtime->mRuntimeID);

        return instance.IsAvailableCpuOk(availCPU, node->GetConfig(), node->NeedBalancing());
    };

    FilterRuntimes(nodes, filter);
}

void Balancer::FilterByRAM(Instance& instance, NodeRuntimes& nodes)
{
    auto filter = [&instance](Node* node, const RuntimeInfo* runtime) {
        auto availRAM = node->GetAvailableRAM(runtime->mRuntimeID);

        return instance.IsAvailableRamOk(availRAM, node->GetConfig(), node->NeedBalancing());
    };

    FilterRuntimes(nodes, filter);
}

void Balancer::FilterByNumInstances(NodeRuntimes& nodes)
{
    auto filter
        = [](Node* node, const RuntimeInfo* runtime) { return !node->IsMaxNumInstancesReached(runtime->mRuntimeID); };

    FilterRuntimes(nodes, filter);
}

void Balancer::FilterTopPriorityNodes(NodeRuntimes& nodes)
{
    if (nodes.IsEmpty()) {
        return;
    }

    using NodeRuntimes = Pair<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>;

    auto topPriorityNode = nodes.Min([](const NodeRuntimes& left, const NodeRuntimes& right) {
        return left.mFirst->GetConfig().mPriority > right.mFirst->GetConfig().mPriority;
    });

    auto topPriority = topPriorityNode->mFirst->GetConfig().mPriority;

    nodes.RemoveIf(
        [topPriority](const NodeRuntimes& item) { return item.mFirst->GetConfig().mPriority != topPriority; });
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
    const auto& scheduledInstances = mInstanceManager->GetScheduledInstances();

    for (const auto& instance : mInstanceManager->GetActiveInstances()) {
        bool isScheduled = scheduledInstances.ContainsIf(
            [&instance](const SharedPtr<Instance>& stashInst) { return stashInst.Get() == instance.Get(); });

        if (!isScheduled) {
            const auto& info = instance->GetInfo();

            if (info.mNodeID.IsEmpty()) {
                // Instance has not been sent to any node(failed to schedule)
                continue;
            }

            auto node = mNodeManager->FindNode(info.mNodeID);
            if (!node) {
                return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
            }

            if (auto err = node->RemoveNetworkParams(info.mInstanceIdent, *mNetworkManager); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error Balancer::SetNetworkParams(bool onlyWithExposedPorts)
{
    for (auto& instance : mInstanceManager->GetScheduledInstances()) {
        const auto& id = instance->GetInfo().mInstanceIdent;

        if (instance->GetInfo().mNodeID.IsEmpty()) {
            continue;
        }

        auto node = mNodeManager->FindNode(instance->GetStatus().mNodeID);
        if (!node) {
            LOG_ERR() << "Can't find node for instance" << Log::Field("instance", id)
                      << Log::Field("nodeID", instance->GetStatus().mNodeID);

            continue;
        }

        auto err = node->SetupNetworkParams(id, onlyWithExposedPorts, *mNetworkManager);
        if (!err.IsNone()) {
            instance->SetError(AOS_ERROR_WRAP(Error(err, "can't setup network params")));

            continue;
        }
    }

    return ErrorEnum::eNone;
}

Error Balancer::SetupNetworkForNewInstances()
{
    for (const auto& node : mNodeManager->GetNodes()) {
        const auto& nodeID = node.GetInfo().mNodeID;

        auto providers = MakeUnique<StaticArray<StaticString<cIDLen>, cMaxNumInstances>>(&mAllocator);

        for (const auto& instance : mInstanceManager->GetScheduledInstances()) {
            if (nodeID == instance->GetInfo().mNodeID) {
                if (auto err = providers->PushBack(instance->GetInfo().mOwnerID); !err.IsNone()) {
                    instance->SetError(AOS_ERROR_WRAP(Error(err, "can't add owner ID")));
                }
            }
        }

        if (auto err = mNetworkManager->UpdateProviderNetwork(*providers, nodeID); !err.IsNone()) {
            return AOS_ERROR_WRAP(Error(err, "can't update provider network"));
        }
    }

    return ErrorEnum::eNone;
}

Error Balancer::PerformPolicyBalancing()
{
    auto imageIndex   = MakeUnique<oci::ImageIndex>(&mAllocator);
    auto instanceInfo = MakeUnique<aos::InstanceInfo>(&mAllocator);

    for (const auto& instance : mInstanceManager->GetScheduledInstances()) {
        const auto& info    = instance->GetInfo();
        const auto& id      = info.mInstanceIdent;
        const auto& version = info.mVersion;

        // Check for running instance
        bool isInstanceRunning
            = !info.mManifestDigest.IsEmpty() && !info.mNodeID.IsEmpty() && !info.mRuntimeID.IsEmpty();
        if (!isInstanceRunning) {
            continue;
        }

        // Load image descriptor
        if (auto err = mImageInfoProvider.GetImageIndex(id.mItemID, version, *imageIndex); !err.IsNone()) {
            LOG_ERR() << "Can't get image index" << Log::Field("instance", id) << Log::Field("version", version)
                      << Log::Field(err);

            continue;
        }

        auto cmpDigest = [&instance](const oci::IndexContentDescriptor& descriptor) {
            return descriptor.mDigest == instance->GetInfo().mManifestDigest;
        };
        auto imageDescriptor = imageIndex->mManifests.FindIf(cmpDigest);

        if (!imageDescriptor) {
            LOG_ERR() << "Can't find image descriptor" << Log::Field("instance", id);

            instance->SetError(AOS_ERROR_WRAP(ErrorEnum::eNotFound));

            continue;
        }

        // Load configs
        auto releaseConfigs = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { instance->ResetConfigs(); });

        if (auto err = instance->LoadConfigs(*imageDescriptor); !err.IsNone()) {
            LOG_ERR() << "Can't load configs" << Log::Field("instance", id) << Log::Field(err);

            instance->SetError(err);

            continue;
        }

        // Check balancing policy
        if (instance->GetBalancingPolicy() != oci::BalancingPolicyEnum::eBalancingDisabled) {
            continue;
        }

        LOG_DBG() << "Perform policy balancing" << Log::Field("instance", id) << Log::Field("nodeID", info.mNodeID);

        auto node = mNodeManager->FindNode(info.mNodeID);
        if (!node) {
            LOG_ERR() << "Can't find node for instance" << Log::Field("instance", id)
                      << Log::Field("nodeID", info.mNodeID);

            instance->SetError(AOS_ERROR_WRAP(ErrorEnum::eFailed));

            continue;
        }

        if (auto err = instance->Schedule(*node, info.mRuntimeID, *instanceInfo); !err.IsNone()) {
            LOG_ERR() << "Can't schedule instance" << Log::Field("instance", id) << Log::Field(err);

            instance->SetError(err);
        }
    }

    return ErrorEnum::eNone;
}

void Balancer::UpdateMonitoringData()
{
    for (auto& node : mNodeManager->GetNodes()) {
        const auto& nodeID = node.GetInfo().mNodeID;

        auto nodeMonitoring = MakeUnique<monitoring::NodeMonitoringData>(&mAllocator);

        if (auto err = mMonitorProvider->GetAverageMonitoring(nodeID, *nodeMonitoring); !err.IsNone()) {
            mInstanceManager->UpdateMonitoringData(nodeMonitoring->mInstances);
        }

        node.UpdateMonitoringData(*nodeMonitoring);
    }
}

Error Balancer::PrepareForBalancing(bool rebalancing)
{
    UpdateMonitoringData();

    mNetworkManager->PrepareForBalancing();

    if (auto err = mInstanceManager->PrepareForBalancing(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNodeManager->PrepareForBalancing(rebalancing); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
