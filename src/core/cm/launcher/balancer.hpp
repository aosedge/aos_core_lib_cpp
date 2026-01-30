/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_BALANCER_HPP_
#define AOS_CORE_CM_LAUNCHER_BALANCER_HPP_

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
        oci::OCISpecItf& ociSpec, NodeManager& nodeManager, MonitoringProviderItf& monitorProvider,
        InstanceRunnerItf& runner, NetworkManager& networkManager);

    /**
     * Runs instances.
     *
     * @param lock lock on the balancing mutex.
     * @param rebalancing flag indicating rebalancing.
     * @return Error.
     */
    Error RunInstances(UniqueLock<Mutex>& lock, bool rebalancing);

private:
    using NodeRuntimes = StaticMap<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>, cMaxNumInstances>;

    static constexpr auto cAllocatorSize = sizeof(StaticArray<RunInstanceRequest, cMaxNumInstances>)
        + sizeof(StaticArray<Node*, cMaxNumNodes>) + sizeof(oci::ItemConfig) + sizeof(oci::ImageConfig)
        + sizeof(oci::ImageIndex) + sizeof(aos::cm::networkmanager::NetworkServiceData) + sizeof(aos::InstanceInfo)
        + sizeof(StaticMap<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>, cMaxNumInstances>)
        + sizeof(StaticArray<StaticString<cIDLen>, cMaxNumInstances>);

    Error PerformNodeBalancing();

    Error ScheduleInstance(Instance& instance, const oci::IndexContentDescriptor& imageDescriptor);

    // Selects nodes
    Error SelectNodes(Instance& instance, Array<Node*>& nodes);
    void  FilterNodesByLabels(Instance& instance, Array<Node*>& nodes);
    void  FilterNodesByResources(Instance& instance, Array<Node*>& nodes);

    // Selects runtime
    RetWithError<Pair<Node*, const RuntimeInfo*>> SelectRuntime(Instance& instance, Array<Node*>& nodes);

    Error CreateRuntimes(Array<Node*>& nodes, NodeRuntimes& runtimes);

    template <typename Filter>
    void FilterRuntimes(NodeRuntimes& runtimes, Filter& filter);
    void FilterByRuntimeType(Instance& instance, NodeRuntimes& runtimes);
    void FilterByPlatform(Instance& instance, NodeRuntimes& runtimes);
    void FilterByCPU(Instance& instance, NodeRuntimes& runtimes);
    void FilterByRAM(Instance& instance, NodeRuntimes& runtimes);
    void FilterByNumInstances(NodeRuntimes& runtimes);
    void FilterTopPriorityNodes(NodeRuntimes& nodes);
    //

    Error UpdateNetwork();
    Error RemoveNetworkForDeletedInstances();
    Error SetNetworkParams(bool onlyWithExposedPorts);
    Error SetupNetworkForNewInstances();

    Error PerformPolicyBalancing();

    Error PrepareForBalancing(bool rebalancing);
    void  UpdateMonitoringData();

    ImageInfoProvider      mImageInfoProvider;
    InstanceManager*       mInstanceManager {};
    NodeManager*           mNodeManager {};
    MonitoringProviderItf* mMonitorProvider {};
    NetworkManager*        mNetworkManager {};
    InstanceRunnerItf*     mRunner {};
    SubjectArray           mSubjects;

    StaticAllocator<cAllocatorSize> mAllocator;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
