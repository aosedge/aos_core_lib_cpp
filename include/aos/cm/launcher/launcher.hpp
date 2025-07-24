/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_LAUNCHER_HPP_
#define AOS_CM_LAUNCHER_LAUNCHER_HPP_

#include <aos/cm/launcher/nodemanager.hpp>
#include <aos/cm/launcher/storage.hpp>

#include <aos/cm/imageprovider.hpp>
#include <aos/cm/networkmanager.hpp>
#include <aos/cm/nodeinfoprovider.hpp>
#include <aos/cm/resourcemanager.hpp>
#include <aos/cm/storagestate/storagestate.hpp>

#include <aos/common/tools/identifierpool.hpp>
#include <aos/common/tools/timer.hpp>

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Launcher configuration.
 */
struct Config {
    /**
     * Nodes connection timeout.
     */
    Duration mNodesConnectionTimeout;

    /**
     * Service TTL.
     */
    Duration mServiceTTL;
};

/**
 * Run instance request.
 */
struct RunInstanceRequest {
    /**
     * Instance identifier.
     */
    InstanceIdent mInstanceID;

    /**
     * Instance labels.
     */
    StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels> mLabels;

    /**
     * Instance priority.
     */
    uint64_t mPriority = 0;
};

/**
 * Run status listener.
 */
class RunStatusListenerItf {
public:
    /**
     * Callback invoked when service run statuses changed.
     *
     * @param runStatus service run statuses.
     */
    virtual void OnRunStatusChanged(const Array<nodemanager::InstanceStatus>& runStatuses) = 0;
};

/**
 * Auxiliary class to accumulate node information.
 */
class NodeHandler {
public:
    /**
     * Run request structure containing services, layers, and instance information.
     */
    struct RunRequest {
        /**
         * Services to run.
         */
        StaticArray<ServiceInfo, cMaxNumInstances> mServices;

        /**
         * Layers required by services.
         */
        StaticArray<LayerInfo, cMaxNumInstances> mLayers;

        /**
         * Instances to run.
         */
        StaticArray<InstanceInfo, cMaxNumInstances> mInstances;
    };

    /**
     * Initializes the node handler.
     *
     * @param nodeInfo node information.
     * @param nodeManager interface manager interface.
     * @param resourceManager resource manager interface.
     * @param isLocalNode true if the node is the local host.
     * @param rebalancing rebalancing indicator.
     * @return Error.
     */
    Error Init(const NodeInfo& nodeInfo, nodemanager::NodeManagerItf& nodeManager,
        resourcemanager::ResourceManagerItf& resourceManager, bool isLocalNode, bool rebalancing);

    /**
     * Updates node data.
     *
     * @param nodeManager interface manager interface.
     * @param resourceManager resource manager interface.
     * @param rebalancing rebalancing indicator.
     * @return Error
     */
    Error UpdateNodeData(nodemanager::NodeManagerItf& nodeManager, resourcemanager::ResourceManagerItf& resourceManager,
        bool rebalancing);

    /**
     * Sets the run status for the node.
     *
     * @param status run status of instances on the node.
     */
    void SetRunStatus(const nodemanager::NodeRunInstanceStatus& status);

    /**
     * Sets the waiting state for the node.
     *
     * @param waiting true if the node is currently waiting.
     */
    void SetWaiting(bool waiting);

    /**
     * Checks if the node is currently waiting.
     *
     * @return bool.
     */
    bool IsWaiting() const;

    /**
     * Checks if the node is the local node.
     *
     * @return bool.
     */
    bool IsLocal() const;

    /**
     * Returns the available size of the specified partition type.
     *
     * @param partitionType type of partition (storage, state).
     * @return uint64_t.
     */
    uint64_t GetPartitionSize(const String& partitionType) const;

    /**
     * Returns the node configuration.
     *
     * @return const NodeConfig&.
     */
    const NodeConfig& GetConfig() const;

    /**
     * Returns node information.
     *
     * @return const NodeInfo&.
     */
    const NodeInfo& GetInfo() const;

    /**
     * Returns the run status of instances on this node.
     *
     * @return const nodemanager::NodeRunInstanceStatus&.
     */
    const nodemanager::NodeRunInstanceStatus& GetRunStatus() const;

    /**
     * Returns running service instances on the node.
     *
     * @return const Array<InstanceInfo>&.
     */
    const Array<InstanceInfo> GetScheduledInstances() const;

    /**
     * Starts scheduled service instances.
     *
     * @param nodeManager interface manager interface.
     * @param forceRestart force restart flag.
     * @return Error.
     */
    Error StartInstances(nodemanager::NodeManagerItf& nodeManager, bool forceRestart);

    /**
     * Stops running service instances.
     *
     * @param nodeManager interface manager interface.
     * @param runningInstances currently running instances.
     * @return Error.
     */
    Error StopInstances(nodemanager::NodeManagerItf& nodeManager, const Array<InstanceIdent>& runningInstances);

    /**
     * Checks if the node supports the given set of devices.
     *
     * @param devices array of devices required by a service.
     * @return bool.
     */
    bool HasDevices(const Array<oci::ServiceDevice>& devices) const;

    /**
     * Adds a run request to the node.
     *
     * @param instance instance information.
     * @param serviceInfo service information.
     * @param layers layers required to run the service.
     * @return Error.
     */
    Error AddRunRequest(const InstanceInfo& instance, const imageprovider::ServiceInfo& serviceInfo,
        const Array<imageprovider::LayerInfo>& layers);

    /**
     * Updates network parameters for a specific instance.
     *
     * @param instance instance identifier.
     * @param params network configuration.
     * @return Error.
     */
    Error UpdateNetworkParams(const InstanceIdent& instance, const NetworkParameters& params);

    /**
     * Returns the total CPU requested by the given instance.
     *
     * @param instance instance identifier.
     * @param serviceConfig service configuration.
     * @return uint64_t.
     */
    uint64_t GetRequestedCPU(const InstanceIdent& instance, const oci::ServiceConfig& serviceConfig) const;

    /**
     * Returns the total RAM requested by the given instance.
     *
     * @param instance instance identifier.
     * @param serviceConfig service configuration.
     * @return uint64_t.
     */
    uint64_t GetRequestedRAM(const InstanceIdent& instance, const oci::ServiceConfig& serviceConfig) const;

    /**
     * Returns the currently available CPU on the node.
     *
     * @return uint64_t.
     */
    uint64_t GetAvailableCPU() const;

    /**
     * Returns the currently available RAM on the node.
     *
     * @return uint64_t.
     */
    uint64_t GetAvailableRAM() const;

    /**
     * Returns the total requested state partition size.
     *
     * @param serviceConfig service configuration.
     * @return uint64_t.
     */
    uint64_t GetReqStateSize(const oci::ServiceConfig& serviceConfig) const;

    /**
     * Returns the total requested storage partition size.
     *
     * @param serviceConfig service configuration.
     * @return uint64_t.
     */
    uint64_t GetReqStorageSize(const oci::ServiceConfig& serviceConfig) const;

    /**
     * Selects and returns nodes sorted by priority.
     *
     * @param inNodes map of node IDs to node handler instances.
     * @return RetWithError<StaticArray<NodeHandler*, cNodeMaxNum>>.
     */
    static RetWithError<StaticArray<NodeHandler*, cNodeMaxNum>> GetNodesByPriorities(
        Map<StaticString<cNodeIDLen>, NodeHandler>& inNodes);

private:
private:
    static constexpr auto cDefaultResourceRation = 50.0;

    static uint64_t ClampResource(uint64_t value, const Optional<uint64_t>& quota);
    static uint64_t GetReqCPUFromNodeConfig(
        const Optional<uint64_t>& quota, const Optional<ResourceRatios>& nodeRatios);
    static uint64_t GetReqRAMFromNodeConfig(
        const Optional<uint64_t>& quota, const Optional<ResourceRatios>& nodeRatios);
    static uint64_t GetReqStateFromNodeConfig(
        const Optional<uint64_t>& quota, const Optional<ResourceRatios>& nodeRatios);
    static uint64_t GetReqStorageFromNodeConfig(
        const Optional<uint64_t>& quota, const Optional<ResourceRatios>& nodeRatios);

    Error ResetDeviceAllocations();
    Error AllocateDevice(const Array<oci::ServiceDevice>& devices);

    void InitAvailableResources(nodemanager::NodeManagerItf& nodeManager, bool rebalancing);
    // Returns CPU usage without Aos service instances.
    uint64_t GetNodeCPU() const;
    // Returns CPU usage without Aos service instances.
    uint64_t GetNodeRAM() const;

    Error AddService(const imageprovider::ServiceInfo& info);
    Error AddLayers(const Array<imageprovider::LayerInfo>& layers);

    NodeInfo                       mInfo;
    bool                           mIsLocal = false;
    NodeConfig                     mConfig;
    monitoring::NodeMonitoringData mAverageMonitoring;

    bool                                                             mIsWaiting       = true;
    bool                                                             mNeedRebalancing = false;
    uint64_t                                                         mAvailableCPU    = 0;
    uint64_t                                                         mAvailableRAM    = 0;
    nodemanager::NodeRunInstanceStatus                               mStatus;
    StaticMap<StaticString<cDeviceNameLen>, int, cMaxNumNodeDevices> mDeviceAllocations;
    RunRequest                                                       mRunRequest;
};

/**
 * Auxiliary class accumulating data about running service instances.
 */
class InstanceManager : public imageprovider::ServiceListenerItf {
public:
    /**
     * Initializes the instance manager.
     *
     * @param config configuration object.
     * @param storage interface to persistent storage.
     * @param imageProvider interface for retrieving service information from its image.
     * @param storageState interface to manage storage and state partitions.
     * @return Error.
     */
    Error Init(const Config& config, storage::StorageItf& storage, imageprovider::ImageProviderItf& imageProvider,
        storagestate::StorageStateItf& storageState);

    /**
     * Starts instance manager.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops instance manager.
     */
    Error Stop();

    /**
     * Updates internal cache of service instances from persistent storage.
     *
     * @return Error.
     */
    Error UpdateInstanceCache();

    /**
     * Sets available space for storage and state partitions
     *
     * @param storageSize available storage partition size in bytes.
     * @param stateSize available state partition size in bytes.
     */
    void SetAvailableStorageStateSize(uint64_t storageSize, uint64_t stateSize);

    /**
     * Callback invoked when a service is removed.
     *
     * @param serviceID service identifier.
     */
    void OnServiceRemoved(const String& serviceID) override;

    /**
     * Retrieves the checksum of a given service instance.
     *
     * @param instanceID service instance identifier.
     * @param[out] checkSum SHA3-224 checksum of the instance.
     * @return Error.
     */
    Error GetInstanceCheckSum(const InstanceIdent& instanceID, String& checkSum);

    /**
     * Caches service instance.
     *
     * @param instance service instance information.
     * @return Error.
     */
    Error CacheInstance(const storage::InstanceInfo& instance);

    /**
     * Marks an instance as failed.
     *
     * @param id service instance identifier.
     * @param serviceVersion version of the service.
     * @param err Error.
     */
    void SetInstanceError(const InstanceIdent& id, const String& serviceVersion, const Error& err);

    /**
     * Retrieves information about a specific instance.
     *
     * @param id service instance identifier.
     * @param[out] info output structure to hold instance information.
     * @return Error.
     */
    Error GetInstanceInfo(const InstanceIdent& id, storage::InstanceInfo& info);

    /**
     * Sets up a new instance based on the run request and service information.
     *
     * @param request instance run request parameters.
     * @param nodeHandler node handler.
     * @param serviceInfo service information.
     * @param rebalancing indicates rebalancing.
     * @param[out] info output service instance information.
     * @return Error.
     */
    Error SetupInstance(const RunInstanceRequest& request, NodeHandler& nodeHandler,
        const imageprovider::ServiceInfo& serviceInfo, bool rebalancing, aos::InstanceInfo& info);

    /**
     * Checks whether a given instance is scheduled to run.
     *
     * @param id service instance identifier.
     * @return true if the instance is scheduled; false otherwise.
     */
    bool IsInstanceScheduled(const InstanceIdent& id) const;

    /**
     * Returns all currently scheduled instances.
     *
     * @return Array<storage::InstanceInfo>.
     */
    const Array<storage::InstanceInfo> GetRunningInstances() const;

    /**
     * Returns status for failed instance.
     *
     * @return <nodemanager::InstanceStatus>.
     */
    const Array<nodemanager::InstanceStatus> GetErrorStatuses() const;

private:
    static constexpr auto cUIDRangeBegin    = 5000;
    static constexpr auto cUIDRangeEnd      = 10000;
    static constexpr auto cMaxNumLockedUIDs = cMaxNumInstances;
    static constexpr auto cRemovePeriod     = Time::cDay;

    Error InitUIDPool();
    Error ClearInstancesWithDeletedServices();
    Error RemoveOutdatedInstances();
    Error RemoveInstance(const storage::InstanceInfo& instance);
    Error SetupInstanceStateStorage(
        const imageprovider::ServiceInfo& serviceInfo, uint64_t reqState, uint64_t reqStorage, InstanceInfo& info);

    Config                           mConfig;
    storage::StorageItf*             mStorage       = nullptr;
    imageprovider::ImageProviderItf* mImageProvider = nullptr;
    storagestate::StorageStateItf*   mStorageState  = nullptr;

    Timer                                                                mCleanInstancesTimer;
    IdentifierRangePool<cUIDRangeBegin, cUIDRangeEnd, cMaxNumLockedUIDs> mUIDPool;

    StaticArray<storage::InstanceInfo, cMaxNumInstances>       mRunInstances;
    StaticArray<nodemanager::InstanceStatus, cMaxNumInstances> mErrorStatus;

    uint64_t mAvailableState   = 0;
    uint64_t mAvailableStorage = 0;

    StaticAllocator<sizeof(StaticArray<storage::InstanceInfo, cMaxNumInstances>) + sizeof(storage::InstanceInfo)
        + sizeof(imageprovider::ServiceInfo) + sizeof(StaticArray<storage::InstanceInfo, cMaxNumInstances>)>
        mAllocator;
};

/**
 * Schedules service instances across multiple nodes in unit.
 */
class ServiceBalancer {
public:
    /**
     * Initializes object instance.
     * @param networkManager interface for managing network parameters for service instances.
     * @param instanceManager service instance manager.
     * @param imageProvider interface that retrieves service information from its image.
     * @param nodeManager interface to manage service instances.
     * @return Error.
     */
    Error Init(networkmanager::NetworkManagerItf& networkManager, InstanceManager& instanceManager,
        imageprovider::ImageProviderItf& imageProvider, nodemanager::NodeManagerItf& nodeManager);

    /**
     * Starts specified service instances.
     *
     * @param instances service instance list.
     * @param rebalancing rebalancing indicator.
     * @return Error.
     */
    Error StartInstances(const Array<RunInstanceRequest>& instances, bool rebalancing);

    /**
     * Stops provided service instances.
     *
     * @param instances service instance list.
     * @return Error.
     */
    Error StopInstances(const Array<storage::InstanceInfo>& instances);

    /**
     * Updates unit nodes.
     *
     * @param nodes node handler list.
     */
    void UpdateNodes(Map<StaticString<cNodeIDLen>, NodeHandler>& nodes);

private:
    static constexpr auto cStoragesPartition = "storages";
    static constexpr auto cStatesPartition   = "states";
    static constexpr auto cBalancingDisable  = "disabled";

    NodeHandler* GetLocalNode();
    Error        UpdateNetworks(const Array<RunInstanceRequest>& instances);
    bool         IsInstanceScheduled(const InstanceIdent& instance);

    void PrepareBalancer();
    void PerformPolicyBalancing(const Array<RunInstanceRequest>& instances);
    void PerformNodeBalancing(const Array<RunInstanceRequest>& instances, bool rebalancing);

    Error GetServiceData(const InstanceIdent& instance, imageprovider::ServiceInfo& serviceInfo,
        Array<imageprovider::LayerInfo>& layers);
    Error GetLayers(const Array<StaticString<cLayerDigestLen>>& digests, Array<imageprovider::LayerInfo>& layers);

    Error FilterNodesByStaticResources(
        const oci::ServiceConfig& serviceConfig, const RunInstanceRequest& request, Array<NodeHandler*>& nodes);
    void FilterActiveNodes(Array<NodeHandler*>& nodes);
    void FilterNodesByRunners(const Array<StaticString<cRunnerNameLen>>& runners, Array<NodeHandler*>& nodes);
    void FilterNodesByLabels(const Array<StaticString<cLabelNameLen>>& runners, Array<NodeHandler*>& nodes);
    void FilterNodesByResources(const Array<StaticString<cResourceNameLen>>& runners, Array<NodeHandler*>& nodes);

    RetWithError<NodeHandler*> SelectNodeForInstance(
        const InstanceIdent& instance, const oci::ServiceConfig& config, Array<NodeHandler*> nodes);
    void FilterNodesByDevices(const Array<oci::ServiceDevice>& devices, Array<NodeHandler*>& nodes);
    void FilterNodesByCPU(
        const InstanceIdent& instance, const oci::ServiceConfig& serviceConfig, Array<NodeHandler*>& nodes);
    void FilterNodesByRAM(
        const InstanceIdent& instance, const oci::ServiceConfig& serviceConfig, Array<NodeHandler*>& nodes);
    void FilterTopPriorityNodes(Array<NodeHandler*>& nodes);

    Error PrepareNetworkForInstances(bool onlyExposedPorts);
    Error PrepareNetworkParams(
        const imageprovider::ServiceInfo& serviceInfo, networkmanager::NetworkInstanceData& params);

    Error SendStartInstances(bool forceRestart);
    Error SendStopInstances(const Array<storage::InstanceInfo>& instances);

    StaticArray<StaticString<cRunnerNameLen>, cMaxNumRunners> mDefaultRunners;

    networkmanager::NetworkManagerItf*          mNetworkManager  = nullptr;
    InstanceManager*                            mInstanceManager = nullptr;
    imageprovider::ImageProviderItf*            mImageProvider   = nullptr;
    nodemanager::NodeManagerItf*                mNodeManager     = nullptr;
    Map<StaticString<cNodeIDLen>, NodeHandler>* mNodes           = nullptr;

    StaticAllocator<sizeof(imageprovider::ServiceInfo) + sizeof(StaticArray<imageprovider::LayerInfo, cMaxNumLayers>)
        + sizeof(storage::InstanceInfo) + sizeof(aos::InstanceInfo)
        + sizeof(StaticArray<InstanceIdent, cMaxNumInstances>)>
        mAllocator;
};

/**
 * Launcher class manages lifecycle of service instances.
 */
class Launcher : private nodemanager::ServiceStatusListenerItf {
public:
    /**
     * Initializes launcher object instance.
     *
     * @param config configuration.
     * @param storage storage interface.
     * @param nodeInfoProvider interface providing information about all unit nodes.
     * @param nodeManager interface to manage service instances.
     * @param imageProvider interface that retrieves service information from its image.
     * @param resourceManager resource manager interface.
     * @param storageState interface to manage storage and state partitions.
     * @param networkManager interface for managing network parameters for service instances.
     * @return Error.
     */
    Error Init(const Config& config, storage::StorageItf& storage,
        nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider, nodemanager::NodeManagerItf& nodeManager,
        imageprovider::ImageProviderItf& imageProvider, resourcemanager::ResourceManagerItf& resourceManager,
        storagestate::StorageStateItf& storageState, networkmanager::NetworkManagerItf& networkManager);

    /**
     * Starts launcher instance.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops launcher instance.
     */
    Error Stop();

    /**
     * Runs given service instances.
     *
     * @param instances service instances.
     * @param rebalancing flag indicating whether .
     * @return Error.
     */
    Error RunInstances(const Array<RunServiceRequest>& instances, bool rebalancing);

    /**
     * Sets service run status listener.
     *
     * @param listener service run status listener.
     */
    void SetListener(RunStatusListenerItf& listener);

    /**
     * Resets service run status listener.
     */
    void ResetListener();

private:
    void OnStatusChanged(const nodemanager::NodeRunInstanceStatus& status) override;

    Error InitNodes(bool rebalancing);
    Error UpdateNodes(bool rebalancing);
    void  SendRunStatus();

    Config                                 mConfig;
    storage::StorageItf*                   mStorage          = nullptr;
    nodeinfoprovider::NodeInfoProviderItf* mNodeInfoProvider = nullptr;
    nodemanager::NodeManagerItf*           mNodeManager      = nullptr;
    imageprovider::ImageProviderItf*       mImageProvider    = nullptr;
    resourcemanager::ResourceManagerItf*   mResourceManager  = nullptr;
    storagestate::StorageStateItf*         mStorageState     = nullptr;
    networkmanager::NetworkManagerItf*     mNetworkManager   = nullptr;

    RunStatusListenerItf*                                                    mRunStatusListener = nullptr;
    Timer                                                                    mConnectionTimer;
    StaticArray<nodemanager::InstanceStatus, cNodeMaxNum * cMaxNumInstances> mRunStatus;
    StaticMap<StaticString<cNodeIDLen>, NodeHandler, cNodeMaxNum>            mNodes;

    InstanceManager mInstanceManager;
    ServiceBalancer mBalancer;

    Mutex mMutex;
    StaticAllocator<sizeof(nodemanager::InstanceStatus) + sizeof(NodeInfo)
        + sizeof(StaticArray<InstanceInfo, cMaxNumInstances>) + sizeof(StaticArray<InstanceIdent, cMaxNumInstances>)
        + sizeof(InstanceIdent) + sizeof(StaticArray<StaticString<cNodeIDLen>, cNodeMaxNum>)>
        mAllocator;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
