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
    Error RunInstances(UniqueLock<Mutex>& lock, Array<SharedPtr<Instance>>& instances, bool rebalancing);

    /**
     * Loads Service Manager (SM) data for active instances that were loaded from storage.
     *
     * @return Error.
     */
    Error LoadSMDataForActiveInstances();

private:
    using NodeRuntimes = StaticMap<Node*, StaticArray<const RuntimeInfo*, cMaxNumNodeRuntimes>, cMaxNumInstances>;

    static constexpr size_t cScheduleInstanceSize
        = sizeof(oci::ImageIndex) + sizeof(StaticArray<Node*, cMaxNumNodes>) + sizeof(NodeRuntimes);
    static constexpr size_t cPolicyBalancingSize = sizeof(oci::ImageIndex);
    static constexpr size_t cNetworkSetupSize    = sizeof(StaticArray<StaticString<cIDLen>, cMaxNumInstances>);
    static constexpr size_t cMonitoringSize      = sizeof(monitoring::NodeMonitoringData);

    static constexpr size_t cAllocatorSize
        = Max(cScheduleInstanceSize, cPolicyBalancingSize, cNetworkSetupSize, cMonitoringSize);

    Error PerformNodeBalancing(Array<SharedPtr<Instance>>& instances);

    Error ScheduleInstance(SharedPtr<Instance>& instance, const oci::IndexContentDescriptor& imageDescriptor);

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

    Error PerformPolicyBalancing(Array<SharedPtr<Instance>>& instances);

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
