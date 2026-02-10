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
#include <core/common/monitoring/itf/monitoringdata.hpp>

#include "itf/instancerunner.hpp"

#include "instance.hpp"
#include "networkmanager.hpp"
#include "nodeitf.hpp"

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Auxiliary class to manage node information.
 */
class Node : public NodeItf {
public:
    /**
     * Initializes node.
     *
     * @param info node information.
     * @param nodeConfigProvider node config provider.
     * @param instanceRunner instance runner interface.
     * @param allocator allocator.
     */
    void Init(const String& id, unitconfig::NodeConfigProviderItf& nodeConfigProvider,
        InstanceRunnerItf& instanceRunner, Allocator* allocator);

    /**
     * Prepares node for balancing.
     *
     * @param monitoringData node monitoring data.
     * @param rebalancing flag indicating rebalancing.
     */
    void PrepareForBalancing(bool rebalancing);

    /**
     * Updates node monitoring data.
     *
     * @param monitoringData node monitoring data.
     */
    void UpdateMonitoringData(const monitoring::NodeMonitoringData& monitoringData);

    /**
     * Returns node information.
     *
     * @return const UnitNodeInfo&.
     */
    const UnitNodeInfo& GetInfo() const { return mInfo; }

    /**
     * Indicates whether node requires rebalancing.
     */
    bool NeedBalancing() const { return mNeedBalancing; }

    /**
     * Updates node information.
     *
     * @param info node information.
     * @return bool.
     */
    bool UpdateInfo(const UnitNodeInfo& info);

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
     * Reserves runtime resources for instance.
     *
     * @param instanceIdent instance identifier (currently unused).
     * @param runtimeID runtime identifier.
     * @param reqCPU requested CPU.
     * @param reqRAM requested RAM.
     * @param reqResources requested shared resources.
     * @return Error.
     */
    Error ReserveResources(const InstanceIdent& instanceIdent, const String& runtimeID, size_t reqCPU, size_t reqRAM,
        const Array<StaticString<cResourceNameLen>>& reqResources) override;

    /**
     * Sends scheduled instances to node.
     *
     * @param scheduledInstances scheduled instances.
     * @param runningInstances running instances.
     * @return Error.
     */
    Error SendScheduledInstances(
        const Array<SharedPtr<Instance>>& scheduledInstances, const Array<InstanceStatus>& runningInstances);

    /**
     * Resends instances to node.
     *
     * @return Error.
     */
    RetWithError<bool> ResendInstances(
        const Array<SharedPtr<Instance>>& scheduledInstances, const Array<InstanceStatus>& runningInstances);

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
    // Returns CPU usage without Aos service instances.
    size_t GetSystemCPUUsage(const monitoring::NodeMonitoringData& monitoringData) const;
    // Returns CPU usage without Aos service instances.
    size_t GetSystemRAMUsage(const monitoring::NodeMonitoringData& monitoringData) const;

    size_t* GetPtrToAvailableCPU(const String& runtimeID);
    size_t* GetPtrToAvailableRAM(const String& runtimeID);
    size_t* GetPtrToMaxNumInstances(const String& runtimeID);

    void Convert(const InstanceStatus& status, aos::InstanceInfo& info);

    unitconfig::NodeConfigProviderItf* mNodeConfigProvider {};
    InstanceRunnerItf*                 mInstanceRunner {};

    UnitNodeInfo mInfo {};
    bool         mNeedBalancing {};

    size_t mTotalCPUUsage {};
    size_t mTotalRAMUsage {};
    size_t mSystemCPUUsage {};
    size_t mSystemRAMUsage {};

    size_t                                          mAvailableCPU {};
    size_t                                          mAvailableRAM {};
    StaticArray<ResourceInfo, cMaxNumNodeResources> mAvailableResources;

    StaticMap<StaticString<cIDLen>, size_t, cMaxNumNodeRuntimes>            mRuntimeAvailableRAM;
    StaticMap<StaticString<cIDLen>, size_t, cMaxNumNodeRuntimes>            mRuntimeAvailableCPU;
    StaticMap<StaticString<cResourceNameLen>, size_t, cMaxNumNodeResources> mMaxInstances;

    Allocator* mAllocator {};
};

/** @}*/
} // namespace aos::cm::launcher

#endif
