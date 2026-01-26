/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_INSTANCE_HPP_
#define AOS_CORE_CM_LAUNCHER_INSTANCE_HPP_

#include <core/cm/storagestate/storagestate.hpp>

#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/identifierpool.hpp>
#include <core/common/types/monitoring.hpp>
#include <core/common/types/unitconfig.hpp>

#include "itf/storage.hpp"

#include "gidpool.hpp"
#include "imageinfoprovider.hpp"
#include "networkmanager.hpp"
#include "nodeitf.hpp"
#include "storagestate.hpp"

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * UID range start.
 */
static constexpr auto cUIDRangeBegin = 5000;

/**
 * UID range end.
 */
static constexpr auto cUIDRangeEnd = 10000;

/**
 * Max number of locked IDs simultaneously.
 */
static constexpr auto cMaxNumLockedUIDs = cMaxNumInstances;

/**
 * User ID pool
 */
using UIDPool = IdentifierRangePool<cUIDRangeBegin, cUIDRangeEnd, cMaxNumLockedUIDs>;

/**
 * Base instance class.
 */
class Instance {
public:
    /**
     * Constructs instance.
     *
     * @param info instance information.
     * @param storage interface to persistent storage.
     * @param imageInfoProvider interface for retrieving service information from image.
     * @param allocator instance allocator.
     */
    Instance(const InstanceInfo& info, StorageItf& storage, ImageInfoProvider& imageInfoProvider, Allocator& allocator);

    /**
     * Destructor.
     */
    virtual ~Instance() = default;

    /**
     * Initializes instance.
     *
     * @return Error.
     */
    virtual Error Init() = 0;

    /**
     * Loads image and (optionally) service configs for the specified manifest descriptor and caches pointers to them.
     *
     * @param imageDescriptor image descriptor.
     * @return Error.
     */
    virtual Error LoadConfigs(const oci::IndexContentDescriptor& imageDescriptor) = 0;

    /**
     * Resets configs.
     */
    virtual void ResetConfigs() = 0;

    /**
     * Returns instance information.
     *
     * @return const InstanceInfo&.
     */
    const InstanceInfo& GetInfo() const { return mInfo; }

    /**
     * Returns instance status.
     *
     * @return const InstanceStatus&.
     */
    const InstanceStatus& GetStatus() const { return mStatus; }

    /**
     * Checks whether image is valid.
     *
     * @return bool.
     */
    bool IsImageValid();

    /**
     * Updates instance status.
     *
     * @param status new status.
     * @return Error.
     */
    Error UpdateStatus(const InstanceStatus& status);

    /**
     * Sets error state.
     *
     * @param err error object.
     * @param resetNodeID whether to reset node ID.
     */
    void SetError(const Error& err, bool resetNodeID = true);

    /**
     * Updates monitoring data.
     *
     * @param monitoringData monitoring data.
     */
    void UpdateMonitoringData(const MonitoringData& monitoringData);

    /**
     * Removes instance.
     *
     * @return Error.
     */
    virtual Error Remove() = 0;

    /**
     * Caches instance.
     *
     * @param disable disable instance.
     * @return Error.
     */
    virtual Error Cache(bool disable = false) = 0;

    /**
     * Checks whether available CPU fits instance requirements.
     *
     * @param availableCPU available CPU.
     * @param nodeConfig node configuration.
     * @param useMonitoringData whether to use monitoring data.
     * @return bool.
     */
    virtual bool IsAvailableCpuOk(size_t availableCPU, const NodeConfig& nodeConfig, bool useMonitoringData) = 0;

    /**
     * Checks whether available RAM fits instance requirements.
     *
     * @param availableRAM available RAM.
     * @param nodeConfig node configuration.
     * @param useMonitoringData whether to use monitoring data.
     * @return bool.
     */
    virtual bool IsAvailableRamOk(size_t availableRAM, const NodeConfig& nodeConfig, bool useMonitoringData) = 0;

    /**
     * Checks whether runtime type fits instance requirements.
     *
     * @param runtimeType runtime type.
     * @return bool.
     */
    virtual bool IsRuntimeTypeOk(const StaticString<cRuntimeTypeLen>& runtimeType) = 0;

    /**
     * Checks whether platform fits instance requirements.
     *
     * @param platformInfo platform info.
     * @return bool.
     */
    bool IsPlatformOk(const PlatformInfo& platformInfo);

    /**
     * Checks whether node resources fit instance requirements.
     *
     * @param resources resources.
     * @return bool.
     */
    virtual bool AreNodeResourcesOk(const ResourceInfoArray& nodeResources) = 0;

    /**
     * Checks whether node labels fit instance requirements.
     *
     * @param nodeLabels node labels.
     * @return bool.
     */
    bool AreNodeLabelsOk(const LabelsArray& nodeLabels);

    /**
     * Returns balancing policy.
     *
     * @return BalancingPolicyEnum.
     */
    virtual oci::BalancingPolicyEnum GetBalancingPolicy() = 0;

    /**
     * Schedules instance on node.
     *
     * @param node node interface.
     * @param runtimeID runtime identifier.
     * @param[out] info preallocate instance info used as temporary location.
     * @return Error.
     */
    virtual Error Schedule(NodeItf& node, const String& runtimeID, aos::InstanceInfo& info) = 0;

protected:
    Error SetActive(const String& nodeID, const String& runtimeID);

    InstanceInfo   mInfo;
    InstanceStatus mStatus;

    StorageItf&        mStorage;
    ImageInfoProvider& mImageInfoProvider;
    Allocator&         mAllocator;

    MonitoringData mMonitoringData;

    UniquePtr<oci::ImageConfig> mImageConfig;
};

/**
 * Component instances.
 */
class ComponentInstance : public Instance {
public:
    /**
     * Constructs component instance.
     *
     * @param info instance information.
     * @param storage interface to persistent storage.
     * @param imageInfoProvider interface for retrieving service information from image.
     * @param allocator instance allocator.
     */
    ComponentInstance(
        const InstanceInfo& info, StorageItf& storage, ImageInfoProvider& imageInfoProvider, Allocator& allocator);

    /**
     * Initializes component instance.
     *
     * @return Error.
     */
    Error Init() override;

    /**
     * Loads instance configs.
     *
     * Component instance loads only image config.
     *
     * @param imageDescriptor image descriptor.
     * @return Error.
     */
    Error LoadConfigs(const oci::IndexContentDescriptor& imageDescriptor) override;

    /**
     * Resets configs.
     */
    void ResetConfigs() override;

    /**
     * Removes component instance.
     *
     * @return Error.
     */
    Error Remove() override;

    /**
     * Caches component instance.
     *
     * @param disable disable instance.
     * @return Error.
     */
    Error Cache(bool disable = false) override;

    /**
     * Checks whether available CPU fits instance requirements.
     *
     * @param availableCPU available CPU.
     * @param nodeConfig node configuration.
     * @param useMonitoringData whether to use monitoring data.
     * @return bool.
     */
    bool IsAvailableCpuOk(size_t availableCPU, const NodeConfig& nodeConfig, bool useMonitoringData) override;

    /**
     * Checks whether available RAM fits instance requirements.
     *
     * @param availableRAM available RAM.
     * @param nodeConfig node configuration.
     * @param useMonitoringData whether to use monitoring data.
     * @return bool.
     */
    bool IsAvailableRamOk(size_t availableRAM, const NodeConfig& nodeConfig, bool useMonitoringData) override;

    /**
     * Checks whether runtime type fits instance requirements.
     *
     * @param runtimeType runtime type.
     * @return bool.
     */
    bool IsRuntimeTypeOk(const StaticString<cRuntimeTypeLen>& runtimeType) override;

    /**
     * Checks whether node resources fit instance requirements.
     *
     * @param nodeResources node resources.
     * @return bool.
     */
    bool AreNodeResourcesOk(const ResourceInfoArray& nodeResources) override;

    /**
     * Returns balancing policy.
     *
     * @return BalancingPolicyEnum.
     */
    oci::BalancingPolicyEnum GetBalancingPolicy() override;

    /**
     * Schedules instance on node.
     *
     * @param node node interface.
     * @param runtimeID runtime identifier.
     * @param[out] info preallocate instance info used as temporary location.
     * @return Error.
     */
    Error Schedule(NodeItf& node, const String& runtimeID, aos::InstanceInfo& info) override;
};

/**
 * Service instances.
 */
class ServiceInstance : public Instance {
public:
    /**
     * Constructs service instance.
     *
     * @param info instance information.
     * @param uidPool pool for managing user identifiers.
     * @param storage interface to persistent storage.
     * @param storageState interface for managing storage and state partitions.
     * @param networkManager interface for managing networks of service instances.
     * @param allocator instance allocator.
     */
    ServiceInstance(const InstanceInfo& info, UIDPool& uidPool, GIDPool& gidPool, StorageItf& storage,
        StorageState& storageState, ImageInfoProvider& imageInfoProvider, NetworkManager& networkManager,
        Allocator& allocator);

    /**
     * Initializes service instance.
     *
     * @return Error.
     */
    Error Init() override;

    /**
     * Loads instance configs.
     *
     * Service instance loads both service and image configs.
     *
     * @param imageDescriptor image descriptor.
     * @return Error.
     */
    Error LoadConfigs(const oci::IndexContentDescriptor& imageDescriptor) override;

    /**
     * Resets configs.
     */
    void ResetConfigs() override;

    /**
     * Removes service instance.
     *
     * @return Error.
     */
    Error Remove() override;

    /**
     * Caches service instance.
     *
     * @param disable disable instance.
     * @return Error.
     */
    Error Cache(bool disable = false) override;

    /**
     * Checks whether available CPU fits instance requirements.
     *
     * @param availableCPU available CPU.
     * @param nodeConfig node configuration.
     * @param useMonitoringData whether to use monitoring data.
     * @return bool.
     */
    bool IsAvailableCpuOk(size_t availableCPU, const NodeConfig& nodeConfig, bool useMonitoringData) override;

    /**
     * Checks whether available RAM fits instance requirements.
     *
     * @param availableRAM available RAM.
     * @param nodeConfig node configuration.
     * @param useMonitoringData whether to use monitoring data.
     * @return bool.
     */
    bool IsAvailableRamOk(size_t availableRAM, const NodeConfig& nodeConfig, bool useMonitoringData) override;

    /**
     * Checks whether runtime type fits instance requirements.
     *
     * @param runtimeType runtime type.
     * @return bool.
     */
    bool IsRuntimeTypeOk(const StaticString<cRuntimeTypeLen>& runtimeType) override;

    /**
     * Checks whether node resources fit instance requirements.
     *
     * @param resources resources.
     * @return bool.
     */
    bool AreNodeResourcesOk(const ResourceInfoArray& resources) override;

    /**
     * Returns balancing policy.
     *
     * @return BalancingPolicyEnum.
     */
    oci::BalancingPolicyEnum GetBalancingPolicy() override;

    /**
     * Schedules instance on node.
     *
     * @param node node interface.
     * @param runtimeID runtime identifier.
     * @param[out] info preallocate instance info used as temporary location.
     * @return Error.
     */
    Error Schedule(NodeItf& node, const String& runtimeID, aos::InstanceInfo& info) override;

private:
    static constexpr auto cDefaultResourceRation = 50.0;

    size_t GetRequestedCPU(const NodeConfig& nodeConfig, bool useMonitoringData);
    size_t GetRequestedRAM(const NodeConfig& nodeConfig, bool useMonitoringData);
    size_t GetReqStateSize(const NodeConfig& nodeConfig);
    size_t GetReqStorageSize(const NodeConfig& nodeConfig);

    size_t ClampResource(size_t value, const Optional<size_t>& quota);
    size_t GetReqCPUFromNodeConfig(const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios);
    size_t GetReqRAMFromNodeConfig(const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios);
    size_t GetReqStateFromNodeConfig(const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios);
    size_t GetReqStorageFromNodeConfig(const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios);

    Error SetupNetworkServiceData();
    Error ReserveRuntimeResources(NodeItf& node);

    Error SetupStateStorage(const NodeConfig& nodeConfig, String& storagePath, String& statePath);

    UniquePtr<oci::ServiceConfig>      mServiceConfig;
    networkmanager::NetworkServiceData mNetworkServiceData {};

    UIDPool&        mUIDPool;
    GIDPool&        mGIDPool;
    StorageState&   mStorageState;
    NetworkManager& mNetworkManager;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
