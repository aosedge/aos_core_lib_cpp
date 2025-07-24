/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aos/cm/launcher/launcher.hpp>

#include "log.hpp"

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ServiceBalancer::Init(networkmanager::NetworkManagerItf& networkManager, InstanceManager& instanceManager,
    imageprovider::ImageProviderItf& imageProvider, nodemanager::NodeManagerItf& nodeManager)
{
    mDefaultRunners.Clear();
    mDefaultRunners.PushBack("crun");
    mDefaultRunners.PushBack("runc");

    mNetworkManager  = &networkManager;
    mInstanceManager = &instanceManager;
    mImageProvider   = &imageProvider;
    mNodeManager     = &nodeManager;

    return ErrorEnum::eNone;
}

Error ServiceBalancer::StartInstances(const Array<RunInstanceRequest>& instances, bool rebalancing)
{
    PrepareBalancer();

    if (auto err = UpdateNetworks(instances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (rebalancing) {
        PerformPolicyBalancing(instances);
    }

    PerformNodeBalancing(instances, rebalancing);

    // first prepare network for instance which have exposed ports
    if (auto err = PrepareNetworkForInstances(true); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // then prepare network for rest of instances
    if (auto err = PrepareNetworkForInstances(false); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNetworkManager->RestartDNSServer(); !err.IsNone()) {
        LOG_ERR() << "Can't restart DNS" << Log::Field(AOS_ERROR_WRAP(err));
    }

    return SendStartInstances(false);
}

Error ServiceBalancer::StopInstances(const Array<storage::InstanceInfo>& instances)
{
    auto networkInstances = MakeUnique<StaticArray<InstanceIdent, cMaxNumInstances>>(&mAllocator);

    if (auto err = mNetworkManager->GetInstances(*networkInstances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& instance : instances) {
        if (networkInstances->Exist(instance.mInstanceID)) {
            if (auto err = mNetworkManager->RemoveInstanceNetworkParameters(instance.mInstanceID, instance.mNodeID);
                !err.IsNone()) {
                LOG_ERR() << "Can't remove network params" << Log::Field(AOS_ERROR_WRAP(err));
            }
        }

        if (auto err = mInstanceManager->CacheInstance(instance); !err.IsNone()) {
            LOG_ERR() << "Can't cache instance" << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    return SendStopInstances(instances);
}

void ServiceBalancer::UpdateNodes(Map<StaticString<cNodeIDLen>, NodeHandler>& nodes)
{
    mNodes = &nodes;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

NodeHandler* ServiceBalancer::GetLocalNode()
{
    auto localNode
        = mNodes->FindIf([](const Pair<StaticString<cNodeIDLen>, NodeHandler>& kv) { return kv.mSecond.IsLocal(); });

    return localNode != mNodes->end() ? &localNode->mSecond : nullptr;
}

void ServiceBalancer::PrepareBalancer()
{
    auto localNode = GetLocalNode();

    if (localNode) {
        auto storageSize = localNode->GetPartitionSize(cStoragesPartition);
        auto stateSize   = localNode->GetPartitionSize(cStatesPartition);

        mInstanceManager->SetAvailableStorageStateSize(storageSize, stateSize);
    } else {
        LOG_ERR() << "Local node not found";
    }
}

Error ServiceBalancer::UpdateNetworks(const Array<RunInstanceRequest>& instances)
{
    auto providers = MakeUnique<StaticArray<StaticString<cProviderIDLen>, cMaxNumServiceProviders>>(&mAllocator);

    for (const auto& id : instances) {
        auto serviceInfo = MakeUnique<imageprovider::ServiceInfo>(&mAllocator);
        if (auto err = mImageProvider->GetServiceInfo(id.mInstanceID.mServiceID, *serviceInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (!providers->Exist(serviceInfo->mProviderID)) {
            if (auto err = providers->EmplaceBack(serviceInfo->mProviderID); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    for (const auto& [nodeID, _] : *mNodes) {
        if (auto err = mNetworkManager->UpdateProviderNetwork(*providers, nodeID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

bool ServiceBalancer::IsInstanceScheduled(const InstanceIdent& instance)
{
    for (const auto& [nodeID, node] : *mNodes) {
        bool exist = node.GetScheduledInstances().ExistIf(
            [&instance](const InstanceInfo& info) { return info.mInstanceIdent == instance; });

        if (exist) {
            return true;
        }

        exist = mInstanceManager->GetErrorStatuses().ExistIf(
            [&instance](const nodemanager::InstanceStatus& status) { return status.mInstanceIdent == instance; });

        if (exist) {
            return true;
        }
    }

    return false;
}

void ServiceBalancer::PerformPolicyBalancing(const Array<RunInstanceRequest>& requests)
{
    auto serviceInfo         = MakeUnique<imageprovider::ServiceInfo>(&mAllocator);
    auto layers              = MakeUnique<StaticArray<imageprovider::LayerInfo, cMaxNumLayers>>(&mAllocator);
    auto storageInstanceInfo = MakeUnique<storage::InstanceInfo>(&mAllocator);
    auto instanceInfo        = MakeUnique<aos::InstanceInfo>(&mAllocator);

    for (const auto& request : requests) {
        const auto& instance = request.mInstanceID;

        LOG_DBG() << "Perform policy balancing" << Log::Field("serviceID", instance.mServiceID)
                  << Log::Field("subjectID", instance.mSubjectID) << Log::Field("instanceID", instance.mInstance);

        if (auto err = GetServiceData(instance, *serviceInfo, *layers); !err.IsNone()) {
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

            continue;
        }

        if (serviceInfo->mConfig.mBalancingPolicy != cBalancingDisable) {
            continue;
        }

        if (auto err = mInstanceManager->GetInstanceInfo(instance, *storageInstanceInfo); !err.IsNone()) {
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

            continue;
        }

        auto node = mNodes->Find(storageInstanceInfo->mNodeID);
        if (node == mNodes->end()) {
            auto err = AOS_ERROR_WRAP(Error(ErrorEnum::eWrongState, "node not found"));
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

            continue;
        }

        if (auto err = mInstanceManager->SetupInstance(request, node->mSecond, *serviceInfo, true, *instanceInfo);
            !err.IsNone()) {
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

            continue;
        }

        if (auto err = node->mSecond.AddRunRequest(*instanceInfo, *serviceInfo, *layers); !err.IsNone()) {
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

            continue;
        }
    }
}

void ServiceBalancer::PerformNodeBalancing(const Array<RunInstanceRequest>& requests, bool rebalancing)
{
    for (const auto& request : requests) {
        const auto& instance = request.mInstanceID;

        auto serviceInfo         = MakeUnique<imageprovider::ServiceInfo>(&mAllocator);
        auto layers              = MakeUnique<StaticArray<imageprovider::LayerInfo, cMaxNumLayers>>(&mAllocator);
        auto storageInstanceInfo = MakeUnique<storage::InstanceInfo>(&mAllocator);
        auto instanceInfo        = MakeUnique<aos::InstanceInfo>(&mAllocator);

        LOG_DBG() << "Perform node balancing" << Log::Field("serviceID", instance.mServiceID)
                  << Log::Field("subjectID", instance.mSubjectID) << Log::Field("instanceID", instance.mInstance);

        if (auto err = GetServiceData(instance, *serviceInfo, *layers); !err.IsNone()) {
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

            continue;
        }

        if (serviceInfo->mConfig.mSkipResourceLimits) {
            LOG_DBG() << "Skip resource limits" << Log::Field("serviceID", instance.mServiceID)
                      << Log::Field("subjectID", instance.mSubjectID);
        }

        auto [nodes, sortErr] = NodeHandler::GetNodesByPriorities(*mNodes);
        if (!sortErr.IsNone()) {
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, sortErr);

            continue;
        }

        if (auto err = FilterNodesByStaticResources(serviceInfo->mConfig, request, nodes); !err.IsNone()) {
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

            continue;
        }

        if (IsInstanceScheduled(instance)) {
            continue;
        }

        if (rebalancing) {
            if (auto err = mInstanceManager->GetInstanceInfo(instance, *storageInstanceInfo); !err.IsNone()) {
                mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

                continue;
            }

            if (storageInstanceInfo->mPrevNodeID != ""
                && storageInstanceInfo->mPrevNodeID != storageInstanceInfo->mNodeID) {
                LOG_DBG() << "Exclude previous node" << Log::Field("prevNodeID", storageInstanceInfo->mPrevNodeID);

                nodes.RemoveIf([&storageInstanceInfo](NodeHandler* node) {
                    return node->GetInfo().mNodeID == storageInstanceInfo->mPrevNodeID;
                });

                if (nodes.IsEmpty()) {
                    auto err = AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "can't find node for rebalancing"));
                    mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

                    continue;
                }
            }
        }

        auto [node, selectErr] = SelectNodeForInstance(instance, serviceInfo->mConfig, nodes);
        if (!selectErr.IsNone()) {
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, selectErr);

            continue;
        }

        if (auto err = mInstanceManager->SetupInstance(request, *node, *serviceInfo, true, *instanceInfo);
            !err.IsNone()) {
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

            continue;
        }

        if (auto err = node->AddRunRequest(*instanceInfo, *serviceInfo, *layers); !err.IsNone()) {
            mInstanceManager->SetInstanceError(instance, serviceInfo->mVersion, err);

            continue;
        }
    }
}

Error ServiceBalancer::GetServiceData(
    const InstanceIdent& instance, imageprovider::ServiceInfo& serviceInfo, Array<imageprovider::LayerInfo>& layers)
{
    if (auto err = mImageProvider->GetServiceInfo(instance.mServiceID, serviceInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (serviceInfo.mState == ServiceStateEnum::eCached) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eWrongState, "service deleted"));
    }

    if (auto err = GetLayers(serviceInfo.mLayerDigests, layers); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ServiceBalancer::GetLayers(
    const Array<StaticString<cLayerDigestLen>>& digests, Array<imageprovider::LayerInfo>& layers)
{
    layers.Clear();

    for (const auto& digest : digests) {
        if (auto err = layers.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = mImageProvider->GetLayerInfo(digest, layers.Back()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

void ServiceBalancer::FilterActiveNodes(Array<NodeHandler*>& nodes)
{
    nodes.RemoveIf([](NodeHandler* node) { return node->GetInfo().mStatus != NodeStatusEnum::eProvisioned; });
}

Error ServiceBalancer::FilterNodesByStaticResources(
    const oci::ServiceConfig& serviceConfig, const RunInstanceRequest& request, Array<NodeHandler*>& nodes)
{
    FilterActiveNodes(nodes);
    if (nodes.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no active nodes"));
    }

    FilterNodesByRunners(serviceConfig.mRunners, nodes);
    if (nodes.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no nodes with service runners"));
    }

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

void ServiceBalancer::FilterNodesByRunners(
    const Array<StaticString<cRunnerNameLen>>& inRunners, Array<NodeHandler*>& nodes)
{
    auto runners = !inRunners.IsEmpty() ? inRunners : mDefaultRunners;

    auto nodeRunners = MakeUnique<StaticArray<StaticString<cRunnerNameLen>, cMaxNumRunners>>(&mAllocator);
    nodes.RemoveIf([&nodeRunners, &runners, this](NodeHandler* node) {
        if (auto err = node->GetInfo().GetRunners(*nodeRunners); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
            LOG_ERR() << "Failed to get node runners" << Log::Field("nodeID", node->GetInfo().mNodeID)
                      << Log::Field(AOS_ERROR_WRAP(err));

            return true;
        }

        for (const auto& nodeRunner : nodeRunners->IsEmpty() ? mDefaultRunners : *nodeRunners) {
            if (runners.Exist(nodeRunner)) {
                return false;
            }
        }

        return true;
    });
}

void ServiceBalancer::FilterNodesByLabels(const Array<StaticString<cLabelNameLen>>& labels, Array<NodeHandler*>& nodes)
{
    if (labels.IsEmpty()) {
        return;
    }

    nodes.RemoveIf([&labels](NodeHandler* node) {
        for (const auto& label : labels) {
            if (!node->GetConfig().mLabels.Exist(label)) {
                return true;
            }
        }

        return false;
    });
}

void ServiceBalancer::FilterNodesByResources(
    const Array<StaticString<cResourceNameLen>>& resources, Array<NodeHandler*>& nodes)
{
    if (resources.IsEmpty()) {
        return;
    }

    nodes.RemoveIf([&resources](NodeHandler* node) {
        for (const auto& resource : resources) {
            auto matchResource = [&resource](const ResourceInfo& info) { return info.mName == resource; };

            if (!node->GetConfig().mResources.ExistIf(matchResource)) {
                return true;
            }
        }

        return false;
    });
}

RetWithError<NodeHandler*> ServiceBalancer::SelectNodeForInstance(
    const InstanceIdent& instance, const oci::ServiceConfig& config, Array<NodeHandler*> nodes)
{
    FilterNodesByDevices(config.mDevices, nodes);
    if (nodes.IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no nodes with requested devices"))};
    }

    FilterNodesByCPU(instance, config, nodes);
    if (nodes.IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no nodes with requested CPU"))};
    }

    FilterNodesByRAM(instance, config, nodes);
    if (nodes.IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no nodes with requested RAM"))};
    }

    FilterTopPriorityNodes(nodes);
    if (nodes.IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "failed top priority nodes filtering"))};
    }

    nodes.Sort([](const NodeHandler* left, const NodeHandler* right) {
        if (left->GetAvailableCPU() < right->GetAvailableCPU()) {
            return false;
        }

        if (left->GetAvailableCPU() > right->GetAvailableCPU()) {
            return true;
        }

        return false;
    });

    return nodes.Front();
}

void ServiceBalancer::FilterNodesByDevices(const Array<oci::ServiceDevice>& devices, Array<NodeHandler*>& nodes)
{
    if (devices.IsEmpty()) {
        return;
    }

    nodes.RemoveIf([&devices](NodeHandler* node) { return !node->HasDevices(devices); });
}

void ServiceBalancer::FilterNodesByCPU(
    const InstanceIdent& instance, const oci::ServiceConfig& serviceConfig, Array<NodeHandler*>& nodes)
{
    nodes.RemoveIf([&instance, &serviceConfig](NodeHandler* node) {
        const auto requestedCPU = node->GetRequestedCPU(instance, serviceConfig);

        LOG_DBG() << "Requested CPU" << Log::Field("nodeID", node->GetInfo().mNodeID)
                  << Log::Field("CPU", requestedCPU);

        return requestedCPU > node->GetAvailableCPU() && !serviceConfig.mSkipResourceLimits;
    });
}

void ServiceBalancer::FilterNodesByRAM(
    const InstanceIdent& instance, const oci::ServiceConfig& serviceConfig, Array<NodeHandler*>& nodes)
{
    nodes.RemoveIf([&instance, &serviceConfig](NodeHandler* node) {
        const auto requestedRAM = node->GetRequestedRAM(instance, serviceConfig);

        LOG_DBG() << "Requested RAM" << Log::Field("nodeID", node->GetInfo().mNodeID)
                  << Log::Field("RAM", requestedRAM);

        return requestedRAM > node->GetAvailableRAM() && !serviceConfig.mSkipResourceLimits;
    });
}

void ServiceBalancer::FilterTopPriorityNodes(Array<NodeHandler*>& nodes)
{
    if (nodes.IsEmpty()) {
        return;
    }

    auto topPriorityNode = nodes.Min([](const NodeHandler* left, const NodeHandler* right) {
        return left->GetConfig().mPriority > right->GetConfig().mPriority;
    });

    nodes.RemoveIf([topPriority = (*topPriorityNode)->GetConfig().mPriority](
                       NodeHandler* node) { return node->GetConfig().mPriority != topPriority; });
}

Error ServiceBalancer::PrepareNetworkForInstances(bool onlyExposedPorts)
{
    auto serviceInfo          = MakeUnique<imageprovider::ServiceInfo>(&mAllocator);
    auto networkManagerParams = MakeUnique<networkmanager::NetworkInstanceData>(&mAllocator);
    auto networkParams        = MakeUnique<NetworkParameters>(&mAllocator);

    auto [nodes, err] = NodeHandler::GetNodesByPriorities(*mNodes);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (auto& node : nodes) {
        for (const auto& instance : node->GetScheduledInstances()) {
            err = mImageProvider->GetServiceInfo(instance.mInstanceIdent.mServiceID, *serviceInfo);
            if (!err.IsNone()) {
                mInstanceManager->SetInstanceError(instance.mInstanceIdent, serviceInfo->mVersion, err);

                continue;
            }

            if (onlyExposedPorts && serviceInfo->mExposedPorts.IsEmpty()) {
                continue;
            }

            err = PrepareNetworkParams(*serviceInfo, *networkManagerParams);
            if (!err.IsNone()) {
                mInstanceManager->SetInstanceError(instance.mInstanceIdent, serviceInfo->mVersion, err);

                continue;
            }

            err = mNetworkManager->PrepareInstanceNetworkParameters(instance.mInstanceIdent, serviceInfo->mProviderID,
                node->GetInfo().mNodeID, *networkManagerParams, *networkParams);
            if (!err.IsNone()) {
                mInstanceManager->SetInstanceError(instance.mInstanceIdent, serviceInfo->mVersion, err);

                continue;
            }

            err = node->UpdateNetworkParams(instance.mInstanceIdent, *networkParams);
            if (!err.IsNone()) {
                mInstanceManager->SetInstanceError(instance.mInstanceIdent, serviceInfo->mVersion, err);

                continue;
            }
        }
    }

    return ErrorEnum::eNone;
}

Error ServiceBalancer::PrepareNetworkParams(
    const imageprovider::ServiceInfo& serviceInfo, networkmanager::NetworkInstanceData& params)
{
    params = networkmanager::NetworkInstanceData {};

    if (serviceInfo.mConfig.mHostname.HasValue()) {
        if (auto err = params.mHosts.PushBack(serviceInfo.mConfig.mHostname.GetValue()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    params.mExposedPorts       = serviceInfo.mExposedPorts;
    params.mAllowedConnections = serviceInfo.mConfig.mAllowedConnections;

    return ErrorEnum::eNone;
}

Error ServiceBalancer::SendStartInstances(bool forceRestart)
{
    const auto& [nodes, sortErr] = NodeHandler::GetNodesByPriorities(*mNodes);
    if (!sortErr.IsNone()) {
        return sortErr;
    }

    Error firstErr = ErrorEnum::eNone;
    for (auto& node : nodes) {
        node->SetWaiting(true);

        auto err = node->StartInstances(*mNodeManager, forceRestart);
        if (!err.IsNone()) {
            err = AOS_ERROR_WRAP(err);

            LOG_ERR() << "Can't run instances" << Log::Field(err);

            if (firstErr.IsNone()) {
                firstErr = err;
            }
        }
    }

    return firstErr;
}

Error ServiceBalancer::SendStopInstances(const Array<storage::InstanceInfo>& instances)
{
    auto nodeInstances = MakeUnique<StaticArray<InstanceIdent, cMaxNumInstances>>(&mAllocator);

    const auto& [nodes, sortErr] = NodeHandler::GetNodesByPriorities(*mNodes);
    if (!sortErr.IsNone()) {
        return sortErr;
    }

    Error firstErr = ErrorEnum::eNone;
    for (auto& node : nodes) {
        nodeInstances->Clear();
        for (const auto& instance : instances) {
            if (instance.mNodeID == node->GetInfo().mNodeID) {
                nodeInstances->PushBack(instance.mInstanceID);
            }
        }

        if (nodeInstances->IsEmpty()) {
            continue;
        }

        node->SetWaiting(true);

        auto err = mNodeManager->StopInstances(node->GetInfo().mNodeID, *nodeInstances);
        if (!err.IsNone()) {
            err = AOS_ERROR_WRAP(err);

            LOG_ERR() << "Can't stop instances" << Log::Field(err);

            if (firstErr.IsNone()) {
                firstErr = err;
            }
        }
    }

    return firstErr;
}

} // namespace aos::cm::launcher
