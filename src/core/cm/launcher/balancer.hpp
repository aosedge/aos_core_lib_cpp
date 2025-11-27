/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_BALANCER_HPP_
#define AOS_CORE_CM_LAUNCHER_BALANCER_HPP_

#include <core/cm/networkmanager/itf/networkmanager.hpp>

#include "itf/instancerunner.hpp"
#include "itf/launcher.hpp"
#include "itf/monitoringprovider.hpp"

#include "imageinfoprovider.hpp"
#include "instancemanager.hpp"
#include "nodemanager.hpp"

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Balances run instances.
 */
class Balancer {
public:
    /**
     * Initializes runner with required managers and providers.
     *
     * @param instanceManager instance manager.
     * @param imageInfoProvider interface for retrieving service information from image.
     * @param nodeManager node manager.
     * @param monitorProvider monitoring provider.
     * @param runner instance runner interface.
     * @param networkManager network manager.
     */
    void Init(InstanceManager& instanceManager, imagemanager::ItemInfoProviderItf& itemInfoProvider,
        imagemanager::BlobInfoProviderItf& blobInfoProvider, oci::OCISpecItf& ociSpec, NodeManager& nodeManager,
        MonitoringProviderItf& monitorProvider, InstanceRunnerItf& runner,
        networkmanager::NetworkManagerItf& networkManager);

    /**
     * Runs instances.
     *
     * @param instances array of run requests.
     * @param rebalancing flag indicating rebalancing.
     * @return Error.
     */
    Error RunInstances(const Array<RunInstanceRequest>& instances, bool rebalancing);

private:
    static constexpr auto cAllocatorSize = sizeof(StaticArray<RunInstanceRequest, cMaxNumInstances>)
        + sizeof(StaticArray<Node*, cMaxNumNodes>) + sizeof(oci::ServiceConfig) + sizeof(oci::ImageConfig)
        + sizeof(oci::ImageIndex) + sizeof(aos::cm::networkmanager::NetworkServiceData) + sizeof(aos::InstanceInfo)
        + sizeof(StaticMap<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>, cMaxNumInstances>)
        + sizeof(StaticArray<StaticString<cIDLen>, cMaxNumInstances>);

    Error SetupInstanceInfo(const oci::ServiceConfig& servConf, const NodeConfig& nodeConf,
        const RunInstanceRequest& request, const oci::IndexContentDescriptor& imageDescriptor, const String& runtimeID,
        const Instance& instance, aos::InstanceInfo& info);

    Error PerformNodeBalancing(const Array<RunInstanceRequest>& instances);

    Error ScheduleInstance(
        Instance& instance, const RunInstanceRequest& request, const oci::IndexContentDescriptor& imageDescriptor);
    Error FilterNodesByStaticResources(
        const oci::ServiceConfig& serviceConfig, const RunInstanceRequest& request, Array<Node*>& nodes);
    void FilterNodesByRuntimes(const Array<StaticString<cRuntimeTypeLen>>& inRuntimes, Array<Node*>& nodes);
    void FilterNodesByLabels(const Array<StaticString<cLabelNameLen>>& labels, Array<Node*>& nodes);
    void FilterNodesByResources(const Array<StaticString<cResourceNameLen>>& resources, Array<Node*>& nodes);

    RetWithError<Pair<Node*, const RuntimeInfo*>> SelectRuntimeForInstance(
        Instance& instance, const oci::ServiceConfig& config, Array<Node*>& nodes);

    Error SelectRuntimes(const Array<StaticString<cRuntimeTypeLen>>& inRuntimes, Array<Node*>& nodes,
        Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& runtimes);
    void  FilterByCPU(Instance& instance, const oci::ServiceConfig& serviceConfig,
         Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& runtimes);
    void  FilterByRAM(Instance& instance, const oci::ServiceConfig& serviceConfig,
         Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& runtimes);
    void  FilterByNumInstances(Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& runtimes);
    void  FilterTopPriorityNodes(Map<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>>& nodes);

    size_t GetRequestedCPU(Instance& instance, const Node& node, const oci::ServiceConfig& serviceConfig);
    size_t GetRequestedRAM(Instance& instance, const Node& node, const oci::ServiceConfig& serviceConfig);

    Error UpdateNetwork();
    Error RemoveNetworkForDeletedInstances();
    Error SetNetworkParams(bool onlyWithExposedPorts);
    Error SetupNetworkForNewInstances();

    Error PerformPolicyBalancing(const Array<RunInstanceRequest>& instances);

    ImageInfoProvider                  mImageInfoProvider;
    InstanceManager*                   mInstanceManager {};
    NodeManager*                       mNodeManager {};
    MonitoringProviderItf*             mMonitorProvider {};
    networkmanager::NetworkManagerItf* mNetworkManager {};
    InstanceRunnerItf*                 mRunner {};
    StaticAllocator<cAllocatorSize>    mAllocator;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
