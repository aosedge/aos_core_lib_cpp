/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_BALANCER_HPP_
#define AOS_CORE_CM_LAUNCHER_BALANCER_HPP_

#include <core/common/tools/memory.hpp>

#include "itf/instancerunner.hpp"
#include "itf/launcher.hpp"
#include "itf/monitoringprovider.hpp"

#include "imageinfoprovider.hpp"
#include "instancemanager.hpp"
#include "nodemanager.hpp"

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
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
     * @param allocator allocator to use for temporary objects.
     * @param instanceManager instance manager.
     * @param imageInfoProvider image info provider.
     * @param nodeManager node manager.
     * @param monitorProvider monitoring provider.
     * @param runner instance runner interface.
     */
    void Init(AllocatorItf& allocator, InstanceManager& instanceManager, ImageInfoProvider& imageInfoProvider,
        NodeManager& nodeManager, MonitoringProviderItf& monitorProvider, InstanceRunnerItf& runner);

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

    Error PerformNodeBalancing(Array<SharedPtr<Instance>>& instances);

    Error ScheduleInstance(SharedPtr<Instance>& instance, const oci::IndexContentDescriptor& imageDescriptor);

    // Selects nodes
    Error SelectNodes(Instance& instance, Array<Node*>& nodes) const;
    void  FilterNodesByID(Instance& instance, Array<Node*>& nodes) const;
    void  FilterNodesByLabels(Instance& instance, Array<Node*>& nodes) const;
    void  FilterNodesByResources(Instance& instance, Array<Node*>& nodes) const;

    // Selects runtime
    RetWithError<Pair<Node*, const RuntimeInfo*>> SelectRuntime(Instance& instance, const Array<Node*>& nodes);

    Error CreateRuntimes(const Array<Node*>& nodes, NodeRuntimes& runtimes) const;

    template <typename Filter>
    void FilterRuntimes(NodeRuntimes& runtimes, Filter& filter) const;
    void FilterByRuntimeType(Instance& instance, NodeRuntimes& runtimes) const;
    void FilterByPlatform(Instance& instance, NodeRuntimes& runtimes) const;
    void FilterByCPU(Instance& instance, NodeRuntimes& runtimes) const;
    void FilterByRAM(Instance& instance, NodeRuntimes& runtimes) const;
    void FilterByNumInstances(NodeRuntimes& runtimes) const;
    void FilterTopPriorityNodes(NodeRuntimes& nodes) const;

    Error PerformPolicyBalancing(Array<SharedPtr<Instance>>& instances);
    Error PrepareForBalancing(bool rebalancing, bool isInitialUpdate = false);
    Error UpdateMonitoringData(bool isInitialUpdate = false);

    ImageInfoProvider*     mImageInfoProvider {};
    InstanceManager*       mInstanceManager {};
    NodeManager*           mNodeManager {};
    MonitoringProviderItf* mMonitorProvider {};
    InstanceRunnerItf*     mRunner {};
    SubjectArray           mSubjects;

    AllocatorItf* mAllocator {};
};

/** @}*/

} // namespace aos::cm::launcher

#endif
