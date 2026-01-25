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
     * Loads instances from storage.
     *
     * @param instances list of sent instances.
     * @return Error.
     */
    Error LoadSentInstances(const Array<SharedPtr<Instance>>& instances);

    /**
     * Updates list of running instances for a node.
     *
     * @param nodeID node identifier.
     * @param statuses list of running instance statuses.
     * @return Error.
     */
    Error UpdateRunnigInstances(const String& nodeID, const Array<InstanceStatus>& statuses);

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
     * Checks whether instance is scheduled.
     *
     * @param instance instance identifier.
     * @return bool.
     */
    bool IsScheduled(const InstanceIdent& instance);

    /**
     * Sends scheduled instances to nodes and waits for instance statuses from them.
     *
     * @param lock mutex lock.
     * @return Error.
     */
    Error SendScheduledInstances(UniqueLock<Mutex>& lock);

    /**
     * Resends instances to nodes and waits for instance statuses from them.
     *
     * @param lock mutex lock.
     * @param updatedNodes updated nodes.
     * @return Error.
     */
    Error ResendInstances(UniqueLock<Mutex>& lock, const Array<StaticString<cIDLen>>& updatedNodes);

private:
    static constexpr auto cStatusUpdateTimeout = Time::cMinutes * 10;
    static constexpr auto cAllocatorSize
        = sizeof(StaticArray<StaticString<cIDLen>, cMaxNumNodes>) + sizeof(UnitNodeInfo);

    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider {};
    unitconfig::NodeConfigProviderItf*     mNodeConfigProvider {};
    InstanceRunnerItf*                     mRunner {};

    StaticAllocator<cAllocatorSize> mAllocator;

    StaticArray<Node, cMaxNumNodes> mNodes;

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> mNodesExpectedToSendStatus;
    ConditionalVariable                             mStatusUpdateCondVar;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
