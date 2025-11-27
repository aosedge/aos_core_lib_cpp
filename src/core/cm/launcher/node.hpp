/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_NODE_HPP_
#define AOS_CORE_CM_LAUNCHER_NODE_HPP_

#include <core/cm/networkmanager/itf/networkmanager.hpp>
#include <core/cm/nodeinfoprovider/itf/nodeinfoprovider.hpp>
#include <core/cm/unitconfig/itf/nodeconfigprovider.hpp>

#include "itf/instancerunner.hpp"
#include "itf/monitoringprovider.hpp"

#include "instance.hpp"

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Auxiliary class to manage node information.
 */
class Node {
public:
    /**
     * Initializes the node handler.
     *
     * @param info node information.
     * @param nodeConfigProvider node config provider.
     * @param instanceRunner instance runner interface.
     * @return Error.
     */
    Error Init(const UnitNodeInfo& info, unitconfig::NodeConfigProviderItf& nodeConfigProvider,
        InstanceRunnerItf& instanceRunner);

    /**
     * Returns node information.
     *
     * @return const UnitNodeInfo&.
     */
    const UnitNodeInfo& GetInfo() const { return mInfo; }

    /**
     * Returns node configuration.
     *
     * @return const NodeConfig&.
     */
    const NodeConfig& GetConfig() const { return mConfig; }

    /**
     * Indicates whether node requires rebalancing.
     */
    bool NeedBalancing() const { return mNeedBalancing; }

    /**
     * Updates available node resources.
     *
     * @param monitoringData node monitoring data.
     * @param rebalancing flag indicating rebalancing.
     */
    void UpdateAvailableResources(const monitoring::NodeMonitoringData& monitoringData, bool rebalancing);

    /**
     * Updates node information.
     *
     * @param info node information.
     */
    void UpdateInfo(const UnitNodeInfo& info);

    /**
     * Updates running instance list.
     *
     * @param instances array of instances.
     * @return Error.
     */
    Error SetRunningInstances(const Array<SharedPtr<Instance>>& instances);

    /**
     * Returns available CPU.
     *
     * @return size_t.
     */
    size_t GetAvailableCPU();

    /**
     * Returns available RAM.
     *
     * @return size_t.
     */
    size_t GetAvailableRAM();

    /**
     * Returns available CPU for runtime.
     *
     * @param runtimeID runtime identifier.
     * @return size_t.
     */
    size_t GetAvailableCPU(const String& runtimeID);

    /**
     * Returns available RAM for runtime.
     *
     * @param runtimeID runtime identifier.
     * @return size_t.
     */
    size_t GetAvailableRAM(const String& runtimeID);

    /**
     * Schedules instance on node.
     *
     * @param instance instance information.
     * @param providerID provider identifier.
     * @param servData network service data.
     * @return Error.
     */
    Error ScheduleInstance(const aos::InstanceInfo& instance, const String& providerID,
        const networkmanager::NetworkServiceData& servData, size_t reqCPU, size_t reqRAM,
        const Array<StaticString<cResourceNameLen>>& reqResources);

    /**
     * Sets up network parameters.
     *
     * @param onlyExposedPorts flag for only exposed ports.
     * @param netMgr network manager.
     * @return Error.
     */
    Error SetupNetworkParams(bool onlyExposedPorts, networkmanager::NetworkManagerItf& netMgr);

    /**
     * Sends update.
     *
     * @return Error.
     */
    Error SendUpdate();

    /**
     * Checks whether instance is running.
     *
     * @param instance instance identifier.
     * @return bool.
     */
    bool IsRunning(const InstanceIdent& instance) const;

    /**
     * Checks whether instance is scheduled.
     *
     * @param instance instance identifier.
     * @return bool.
     */
    bool IsScheduled(const InstanceIdent& instance) const;

    /**
     * Checks whether max number of instances is reached.
     *
     * @param runtimeID runtime identifier.
     * @return bool.
     */
    bool IsMaxNumInstancesReached(const String& runtimeID);

    /**
     * Updates node config.
     */
    void UpdateConfig();

private:
    static constexpr auto cMaxNumInstancesPerNode = cMaxNumInstances / cMaxNumNodes;
    static constexpr auto cAllocatorSize
        = sizeof(aos::InstanceInfo) + sizeof(StaticArray<aos::InstanceInfo, cMaxNumInstancesPerNode>);

    void Convert(const InstanceStatus& src, aos::InstanceInfo& dst);

    // Returns CPU usage without Aos service instances.
    size_t GetSystemCPUUsage(const monitoring::NodeMonitoringData& monitoringData) const;
    // Returns CPU usage without Aos service instances.
    size_t GetSystemRAMUsage(const monitoring::NodeMonitoringData& monitoringData) const;

    size_t* GetPtrToAvailableCPU(const String& runtimeID);
    size_t* GetPtrToAvailableRAM(const String& runtimeID);
    size_t* GetPtrToMaxNumInstances(const String& runtimeID);

    unitconfig::NodeConfigProviderItf* mNodeConfigProvider {};
    InstanceRunnerItf*                 mInstanceRunner {};

    UnitNodeInfo mInfo {};
    NodeConfig   mConfig {};
    bool         mNeedBalancing {};

    StaticArray<aos::InstanceInfo, cMaxNumInstancesPerNode> mRunningInstances;
    // cppcheck-suppress templateRecursion
    StaticArray<aos::InstanceInfo, cMaxNumInstancesPerNode>                  mScheduledInstances;
    StaticArray<StaticString<cIDLen>, cMaxNumInstancesPerNode>               mOwnerIDs;
    StaticArray<networkmanager::NetworkServiceData, cMaxNumInstancesPerNode> mNetworkServiceData;

    size_t                                          mAvailableCPU {};
    size_t                                          mAvailableRAM {};
    StaticArray<ResourceInfo, cMaxNumNodeResources> mAvailableResources;

    StaticMap<StaticString<cIDLen>, size_t, cMaxNumNodeRuntimes>            mRuntimeAvailableRAM;
    StaticMap<StaticString<cIDLen>, size_t, cMaxNumNodeRuntimes>            mRuntimeAvailableCPU;
    StaticMap<StaticString<cResourceNameLen>, size_t, cMaxNumNodeResources> mMaxInstances;

    StaticAllocator<cAllocatorSize> mAllocator;
};

/** @}*/
} // namespace aos::cm::launcher

#endif
