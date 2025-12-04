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
class NodeManager : public nodeinfoprovider::NodeInfoListenerItf {
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
        unitconfig::NodeConfigProviderItf& nodeConfigProvider, storagestate::StorageStateItf& storageState,
        InstanceRunnerItf& runner);

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
     * Updates instances for all nodes.
     *
     * @param instances instances.
     * @return Error.
     */
    Error UpdateNodeInstances(const String& nodeID, const Array<SharedPtr<Instance>>& instances);

    /**
     * Returns nodes ordered by priorities.
     *
     * @param nodes output array of nodes.
     * @return Error.
     */
    Error GetNodesByPriorities(Array<Node*>& nodes);

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
     * Sets up state storage for instance.
     *
     * @param nodeConfig node configuration.
     * @param serviceConfig service configuration.
     * @param gid group identifier.
     * @param info instance information.
     * @return Error.
     */
    Error SetupStateStorage(
        const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig, gid_t gid, aos::InstanceInfo& info);

    /**
     * Checks whether instance is running.
     *
     * @param instance instance identifier.
     * @return bool.
     */
    bool IsRunning(const InstanceIdent& instance);

    /**
     * Checks whether instance is scheduled.
     *
     * @param instance instance identifier.
     * @return bool.
     */
    bool IsScheduled(const InstanceIdent& instance);

    /**
     * Synchronous call that sends update instance request and waits for instance statuses from nodes.
     *
     * @param timeout timeout.
     * @return Error.
     */
    Error SendUpdate(const Duration& timeout, UniqueLock<Mutex>& lock);

private:
    static constexpr auto cDefaultResourceRation = 50.0;
    static constexpr auto cStatusUpdateTimeout   = Time::cMinutes * 10;
    static constexpr auto cAllocatorSize
        = sizeof(StaticArray<StaticString<cIDLen>, cMaxNumNodes>) + sizeof(UnitNodeInfo) + sizeof(size_t) * 2;

    /**
     * Handler of node info change event.
     */
    void OnNodeInfoChanged(const UnitNodeInfo& info) override;

    size_t GetReqStateSize(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig) const;
    size_t GetReqStorageSize(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig) const;

    size_t ClampResource(size_t value, const Optional<size_t>& quota) const;
    size_t GetReqStateFromNodeConfig(const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios) const;
    size_t GetReqStorageFromNodeConfig(const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios) const;

    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider {};
    unitconfig::NodeConfigProviderItf*     mNodeConfigProvider {};
    storagestate::StorageStateItf*         mStorageStateManager {};
    InstanceRunnerItf*                     mRunner {};

    StaticAllocator<cAllocatorSize> mAllocator;
    SharedPtr<size_t>               mAvailableState;
    SharedPtr<size_t>               mAvailableStorage;

    StaticArray<Node, cMaxNumNodes> mNodes;

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> mNodesExpectedToSendStatus;
    ConditionalVariable                             mStatusUpdateCondVar;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
