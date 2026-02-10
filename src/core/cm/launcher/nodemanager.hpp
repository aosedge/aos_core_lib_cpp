/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_NODEMANAGER_HPP_
#define AOS_CORE_CM_LAUNCHER_NODEMANAGER_HPP_

#include <core/cm/nodeinfoprovider/itf/nodeinfoprovider.hpp>
#include <core/cm/storagestate/storagestate.hpp>
#include <core/common/tools/allocator.hpp>
#include <core/common/tools/thread.hpp>

#include "node.hpp"

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Auxiliary to communicate with nodes on the unit.
 */
class NodeManager {
public:
    /**
     * Initializes node manager.
     *
     * @param nodeInfoProvider node info provider.
     * @param nodeConfigProvider node config provider.
     * @param storageState storage state interface.
     * @param runner instance runner interface.
     */
    void Init(nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
        unitconfig::NodeConfigProviderItf& nodeConfigProvider, InstanceRunnerItf& runner);

    /**
     * Starts node manager.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops node manager.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Prepares node manager for balancing.
     *
     * @return Error.
     */
    Error PrepareForBalancing(bool rebalancing);

    /**
     * Loads SM data for active instances that were loaded from storage.
     *
     * When instances are created and scheduled normally, the SM data (aos::InstanceInfo) is populated
     * during the Schedule() step. This includes network parameters, storage paths, monitoring params,
     * and other information required by the Service Manager to manage the instance.
     *
     * However, when the system starts and loads active instances from persistent storage, these
     * instances bypass the normal scheduling flow. As a result, their SM data is not automatically
     * populated, even though the instances are already running on their respective nodes.
     *
     * @param activeInstances list of active instances.
     * @param imageInfoProvider image info provider.
     * @return Error.
     */
    Error LoadSMDataForActiveInstances(
        const Array<SharedPtr<Instance>>& activeInstances, ImageInfoProvider& imageInfoProvider);

    /**
     * Notifies that node status has been received.
     *
     * @param nodeID node identifier.
     * @return Error.
     */
    Error NotifyNodeStatusReceived(const String& nodeID);

    /**
     * Updates node info.
     *
     * @param info node information.
     * @return bool.
     */
    bool UpdateNodeInfo(const UnitNodeInfo& info);

    /**
     * Returns connected nodes ordered by priorities.
     *
     * @param nodes output array of nodes.
     * @return Error.
     */
    Error GetConnectedNodes(Array<Node*>& nodes);

    /**
     * Finds node by identifier.
     *
     * @param nodeID node identifier.
     * @return Node*.
     */
    Node* FindNode(const String& nodeID);

    /**
     * Returns nodes.
     *
     * @return Array<Node>&.
     */
    Array<Node>& GetNodes();

    /**
     * Sends scheduled instances to nodes and waits for instance statuses from them.
     *
     * @param lock mutex lock.
     * @param scheduledInstances scheduled instances.
     * @param runningInstances running instances.
     * @return Error.
     */
    Error SendScheduledInstances(UniqueLock<Mutex>& lock, const Array<SharedPtr<Instance>>& scheduledInstances,
        const Array<InstanceStatus>& runningInstances);

    /**
     * Resends instances to nodes and waits for instance statuses from them.
     *
     * @param lock mutex lock.
     * @param updatedNodes updated nodes.
     * @param activeInstances active instances.
     * @param runningInstances running instances.
     * @return Error.
     */
    Error ResendInstances(UniqueLock<Mutex>& lock, const Array<StaticString<cIDLen>>& updatedNodes,
        const Array<SharedPtr<Instance>>& activeInstances, const Array<InstanceStatus>& runningInstances);

private:
    static constexpr auto cStatusUpdateTimeout = Time::cMinutes * 10;

    static constexpr auto cAllocatorSize
        = sizeof(StaticArray<StaticString<cIDLen>, cMaxNumNodes>) + sizeof(UnitNodeInfo);

    static constexpr auto cNodeAllocatorSize = sizeof(StaticArray<aos::InstanceInfo, cMaxNumInstances>) * 2;

    Error FindImageDescriptor(const String& itemID, const String& version, const String& manifestDigest,
        ImageInfoProvider& imageInfoProvider, oci::IndexContentDescriptor& imageDescriptor);

    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider {};
    unitconfig::NodeConfigProviderItf*     mNodeConfigProvider {};
    InstanceRunnerItf*                     mRunner {};

    StaticAllocator<cAllocatorSize>     mAllocator;
    StaticAllocator<cNodeAllocatorSize> mNodeAllocator;

    StaticArray<Node, cMaxNumNodes> mNodes;

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> mNodesExpectedToSendStatus;
    ConditionalVariable                             mStatusUpdateCondVar;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
