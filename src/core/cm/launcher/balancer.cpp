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
    InstanceRunnerItf& runner, networkmanager::NetworkManagerItf& networkManager)
{
    mInstanceManager = &instanceManager;
    mImageInfoProvider.Init(itemInfoProvider, ociSpec);
    mNodeManager     = &nodeManager;
    mMonitorProvider = &monitorProvider;
    mRunner          = &runner;
    mNetworkManager  = &networkManager;
}

RetWithError<bool> Balancer::SetSubjects(const Array<StaticString<cIDLen>>& subjects)
{
    if (auto err = mSubjects.Assign(subjects); !err.IsNone()) {
        return {false, AOS_ERROR_WRAP(err)};
    }

    for (const auto& instance : mInstanceManager->GetActiveInstances()) {
        if (!IsSubjectEnabled(*instance)) {
            return {true, ErrorEnum::eNone};
        }
    }

    for (const auto& instance : mInstanceManager->GetCachedInstances()) {
        if (IsSubjectEnabled(*instance) && instance->GetInfo().mState == InstanceStateEnum::eDisabled) {
            return {true, ErrorEnum::eNone};
        }
    }

    return {false, ErrorEnum::eNone};
}

Error Balancer::RunInstances(const Array<RunInstanceRequest>& instances, UniqueLock<Mutex>& lock, bool rebalancing)
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

    if (auto err = mInstanceManager->SubmitStash(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNodeManager->SendScheduledInstances(lock); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Balancer::SetupInstanceInfo(const oci::ServiceConfig& servConf, const NodeConfig& nodeConf,
    const RunInstanceRequest& request, const oci::IndexContentDescriptor& imageDescriptor, const String& runtimeID,
    const Instance& instance, aos::InstanceInfo& info)
{
    // create instance info, InstanceNetworkParameters are added after network updates
    static_cast<InstanceIdent&>(info) = instance.GetInfo().mInstanceIdent;
    info.mManifestDigest              = imageDescriptor.mDigest;
    info.mRuntimeID                   = runtimeID;
    info.mOwnerID                     = request.mOwnerID;
    info.mPriority                    = request.mPriority;
    info.mUID                         = instance.GetInfo().mUID;
    info.mGID                         = instance.GetInfo().mGID;
    info.mSubjectType                 = request.mSubjectInfo.mSubjectType;
    info.mNetworkParameters.EmplaceValue();

    if (auto err = mNodeManager->SetupStateStorage(nodeConf, servConf, info.mGID, info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Balancer::PerformNodeBalancing(const Array<RunInstanceRequest>& requests)
{
    LOG_DBG() << "Perform node balancing" << Log::Field("reqNum", requests.Size());

    for (const auto& request : requests) {
        LOG_DBG() << "Perform node balancing" << Log::Field("itemID", request.mItemID)
                  << Log::Field("numInstances", request.mNumInstances);

        for (size_t i = 0; i < request.mNumInstances; i++) {
            InstanceIdent instanceIdent {request.mItemID, request.mSubjectInfo.mSubjectID, i, request.mUpdateItemType};

            if (mNodeManager->IsScheduled(instanceIdent)) {
                LOG_DBG() << "Instance aready scheduled" << Log::Field("instance", instanceIdent);

                continue;
            }

            if (auto err = mInstanceManager->AddInstanceToStash(instanceIdent, request); !err.IsNone()) {
                LOG_ERR() << "Can't create new instance" << Log::Field("instance", instanceIdent.mItemID)
                          << Log::Field(err);

                continue;
            }

            auto instance = mInstanceManager->FindStashInstance(instanceIdent);

            if (!IsSubjectEnabled(**instance)) {
                LOG_DBG() << "Subject disabled" << Log::Field("instance", instanceIdent.mItemID);

                mInstanceManager->DisableInstance(*instance);

                continue;
            }

            auto imageIndex = MakeUnique<oci::ImageIndex>(&mAllocator);

            if (auto err = mImageInfoProvider.GetImageIndex(instanceIdent.mItemID, request.mVersion, *imageIndex);
                !err.IsNone()) {
                LOG_ERR() << "Can't get images" << Log::Field("instance", instanceIdent.mItemID) << Log::Field(err);

                continue;
            }

            Error scheduleErr = ErrorEnum::eNone;

            for (const auto& manifest : imageIndex->mManifests) {
                LOG_DBG() << "Try to schedule instance" << Log::Field("instance", (*instance)->GetInfo().mInstanceIdent)
                          << Log::Field("manifest", manifest.mDigest);

                if (auto err = ScheduleInstance(**instance, request, manifest); err.IsNone()) {
                    LOG_DBG() << "Instance scheduled successfully";

                    break;
                } else {
                    LOG_ERR() << "Can't schedule instance" << Log::Field(err);

                    scheduleErr = err;
                }
            }

            if (!scheduleErr.IsNone()) {
                (*instance)->SetError(scheduleErr);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error Balancer::ScheduleInstance(
    Instance& instance, const RunInstanceRequest& request, const oci::IndexContentDescriptor& imageDescriptor)
{
    auto nodes         = MakeUnique<StaticArray<Node*, cMaxNumNodes>>(&mAllocator);
    auto serviceConfig = MakeUnique<oci::ServiceConfig>(&mAllocator);
    auto imageConfig   = MakeUnique<oci::ImageConfig>(&mAllocator);

    // Get service and image configs.
    if (auto err = mImageInfoProvider.GetServiceConfig(imageDescriptor, *serviceConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "get service config failed"));
    }

    if (auto err = mImageInfoProvider.GetImageConfig(imageDescriptor, *imageConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "get image config failed"));
    }

    // Select node runtimes.
    if (auto err = mNodeManager->GetConnectedNodes(*nodes); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "get connected nodes failed"));
    }

    if (auto err = FilterNodesByStaticResources(*serviceConfig, request, *nodes); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "can't find node for instance"));
    }

    auto [nodeRuntime, selectErr] = SelectRuntimeForInstance(instance, *serviceConfig, *imageConfig, *nodes);
    if (!selectErr.IsNone()) {
        return AOS_ERROR_WRAP(Error(selectErr, "can't select runtime for instance"));
    }

    auto&       node    = nodeRuntime.mFirst;
    const auto& runtime = nodeRuntime.mSecond;

    // Create network params.
    auto networkServiceData = MakeUnique<aos::cm::networkmanager::NetworkServiceData>(&mAllocator);

    networkServiceData->mExposedPorts       = imageConfig->mConfig.mExposedPorts;
    networkServiceData->mAllowedConnections = serviceConfig->mAllowedConnections;
    if (serviceConfig->mHostname.HasValue()) {
        networkServiceData->mHosts.PushBack(serviceConfig->mHostname.GetValue());
    }

    // Create instance info, InstanceNetworkParameters will be added after network update.
    auto instanceInfo = MakeUnique<aos::InstanceInfo>(&mAllocator);

    if (auto err = SetupInstanceInfo(
            *serviceConfig, node->GetConfig(), request, imageDescriptor, runtime->mRuntimeID, instance, *instanceInfo);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "can't setup instance info"));
    }

    // Schedule instance.
    auto reqCPU       = GetRequestedCPU(instance, *node, *serviceConfig);
    auto reqRAM       = GetRequestedRAM(instance, *node, *serviceConfig);
    auto reqResources = serviceConfig->mResources;

    if (auto err = node->ScheduleInstance(*instanceInfo, *networkServiceData, reqCPU, reqRAM, reqResources);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "can't schedule instance"));
    }

    if (auto err = instance.Schedule(*instanceInfo, node->GetInfo().mNodeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "can't schedule instance"));
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

RetWithError<Pair<Node*, const RuntimeInfo*>> Balancer::SelectRuntimeForInstance(Instance& instance,
    const oci::ServiceConfig& serviceConfig, const oci::ImageConfig& imageConfig, Array<Node*>& nodes)
{
    auto nodeRuntimes
        = MakeUnique<StaticMap<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>, cMaxNumInstances>>(
            &mAllocator);

    if (auto err = FilterRuntimes(imageConfig, serviceConfig, nodes, *nodeRuntimes); !err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    FilterByCPU(instance, serviceConfig, *nodeRuntimes);
    if (nodeRuntimes->IsEmpty()) {
        return {nullptr, AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "no runtimes with requested CPU"))};
    }

    FilterByRAM(instance, serviceConfig, *nodeRuntimes);
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

Error Balancer::FilterRuntimes(const oci::ImageConfig& imageConfig, const oci::ServiceConfig& serviceConfig,
    Array<Node*>& nodes, Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& runtimes)
{
    for (auto node : nodes) {
        if (auto err = runtimes.Emplace(node); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& suitableNodeRuntimes = runtimes.Find(node)->mSecond;

        for (const auto& nodeRuntime : node->GetInfo().mRuntimes) {
            // Required params: runtime type, OS, architecture
            if (!serviceConfig.mRuntimes.Contains(nodeRuntime.mRuntimeType)) {
                continue;
            }

            if (nodeRuntime.mOSInfo.mOS != imageConfig.mOS
                || nodeRuntime.mArchInfo.mArchitecture != imageConfig.mArchitecture) {
                continue;
            }

            // Optional params: architecture variant, OS version, OS features
            if (!imageConfig.mVariant.IsEmpty()) {
                if (nodeRuntime.mArchInfo.mVariant != imageConfig.mVariant) {
                    continue;
                }
            }

            if (!imageConfig.mOSVersion.IsEmpty()) {
                if (nodeRuntime.mOSInfo.mVersion != imageConfig.mOSVersion) {
                    continue;
                }
            }

            if (!imageConfig.mOSFeatures.IsEmpty()) {
                bool allFeaturesExist = true;

                for (const auto& imageFeature : imageConfig.mOSFeatures) {
                    bool exists = nodeRuntime.mOSInfo.mFeatures.Contains(imageFeature);
                    if (!exists) {
                        allFeaturesExist = false;
                        break;
                    }
                }

                if (!allFeaturesExist) {
                    continue;
                }
            }

            // Add runtime
            if (auto err = suitableNodeRuntimes.PushBack(&nodeRuntime); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        // Remove node if no suitable runtimes
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
        auto err
            = node.SetupNetworkParams(onlyWithExposedPorts, *mNetworkManager, mInstanceManager->GetStashInstances());
        if (!err.IsNone()) {
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
                if (auto err = providers->PushBack(instance->GetOwnerID()); !err.IsNone()) {
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

Error Balancer::PerformPolicyBalancing(const Array<RunInstanceRequest>& requests)
{
    for (const auto& request : requests) {
        auto imageIndex    = MakeUnique<oci::ImageIndex>(&mAllocator);
        auto serviceConfig = MakeUnique<oci::ServiceConfig>(&mAllocator);
        auto imageConfig   = MakeUnique<oci::ImageConfig>(&mAllocator);

        if (auto err = mImageInfoProvider.GetImageIndex(request.mItemID, request.mVersion, *imageIndex);
            !err.IsNone()) {
            LOG_ERR() << "Can't get image index" << Log::Field("itemID", request.mItemID) << Log::Field(err);

            continue;
        }

        for (size_t i = 0; i < request.mNumInstances; i++) {
            InstanceIdent instanceIdent {request.mItemID, request.mSubjectInfo.mSubjectID, i, request.mUpdateItemType};

            if (!mNodeManager->IsRunning(instanceIdent)) {
                continue;
            }

            auto instance = mInstanceManager->FindActiveInstance(instanceIdent);
            if (!instance) {
                LOG_ERR() << "Can't find instance" << Log::Field("instance", instanceIdent);

                continue;
            }

            if (!IsSubjectEnabled(**instance)) {
                LOG_DBG() << "Subject disabled" << Log::Field("instance", instanceIdent);

                mInstanceManager->DisableInstance(*instance);

                continue;
            }

            auto imageDescriptor
                = imageIndex->mManifests.FindIf([&instance](const oci::IndexContentDescriptor& descriptor) {
                      return descriptor.mDigest == (*instance)->GetInfo().mManifestDigest;
                  });

            if (!imageDescriptor) {
                LOG_ERR() << "Can't find image descriptor" << Log::Field("instance", instanceIdent);

                (*instance)->SetError(AOS_ERROR_WRAP(ErrorEnum::eNotFound));

                continue;
            }

            if (auto err = mImageInfoProvider.GetServiceConfig(*imageDescriptor, *serviceConfig); !err.IsNone()) {
                LOG_ERR() << "Can't get service config" << Log::Field("instance", instanceIdent) << Log::Field(err);

                (*instance)->SetError(AOS_ERROR_WRAP(err));

                continue;
            }

            if (serviceConfig->mBalancingPolicy != oci::BalancingPolicyEnum::eBalancingDisabled) {
                continue;
            }

            LOG_DBG() << "Perform policy balancing" << Log::Field("instance", instanceIdent);

            if (auto err = mImageInfoProvider.GetImageConfig(*imageDescriptor, *imageConfig); !err.IsNone()) {
                LOG_ERR() << "Can't get image config" << Log::Field("instance", instanceIdent) << Log::Field(err);

                (*instance)->SetError(AOS_ERROR_WRAP(err));

                continue;
            }

            auto addInstanceErr = mInstanceManager->AddInstanceToStash(instanceIdent, request);
            if (!addInstanceErr.IsNone()) {
                LOG_ERR() << "Can't add instance to stash" << Log::Field("instance", instanceIdent)
                          << Log::Field(addInstanceErr);

                (*instance)->SetError(addInstanceErr);

                continue;
            }

            auto networkServiceData = MakeUnique<networkmanager::NetworkServiceData>(&mAllocator);

            networkServiceData->mExposedPorts       = imageConfig->mConfig.mExposedPorts;
            networkServiceData->mAllowedConnections = serviceConfig->mAllowedConnections;
            if (serviceConfig->mHostname.HasValue()) {
                networkServiceData->mHosts.PushBack(serviceConfig->mHostname.GetValue());
            }

            auto node = mNodeManager->FindNode((*instance)->GetInfo().mNodeID);
            if (!node) {
                LOG_ERR() << "Can't find node" << Log::Field("instance", instanceIdent);

                (*instance)->SetError(AOS_ERROR_WRAP(ErrorEnum::eFailed));

                continue;
            }

            auto instanceInfo = MakeUnique<aos::InstanceInfo>(&mAllocator);

            if (auto err = SetupInstanceInfo(*serviceConfig, node->GetConfig(), request, *imageDescriptor,
                    (*instance)->GetInfo().mRuntimeID, **instance, *instanceInfo);
                !err.IsNone()) {

                LOG_ERR() << "Can't setup instance info" << Log::Field("instance", instanceIdent) << Log::Field(err);

                (*instance)->SetError(err);

                continue;
            }

            auto reqCPU       = GetRequestedCPU(**instance, *node, *serviceConfig);
            auto reqRAM       = GetRequestedRAM(**instance, *node, *serviceConfig);
            auto reqResources = serviceConfig->mResources;

            if (auto err = node->ScheduleInstance(*instanceInfo, *networkServiceData, reqCPU, reqRAM, reqResources);
                !err.IsNone()) {
                LOG_ERR() << "Can't schedule instance" << Log::Field("instance", instanceIdent) << Log::Field(err);

                (*instance)->SetError(err);

                continue;
            }

            if (auto err = (*instance)->Schedule(*instanceInfo, node->GetInfo().mNodeID); !err.IsNone()) {
                LOG_ERR() << "Can't schedule instance" << Log::Field("instance", instanceIdent) << Log::Field(err);

                (*instance)->SetError(err);

                continue;
            }
        }
    }

    return ErrorEnum::eNone;
}

bool Balancer::IsSubjectEnabled(const Instance& instance)
{
    return !instance.GetInfo().mIsUnitSubject || mSubjects.Contains(instance.GetInfo().mInstanceIdent.mSubjectID);
}

} // namespace aos::cm::launcher
