/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_INSTANCE_HPP_
#define AOS_CORE_CM_LAUNCHER_INSTANCE_HPP_

#include <core/cm/storagestate/storagestate.hpp>

#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>
#include <core/common/monitoring/monitoring.hpp>
#include <core/common/tools/identifierpool.hpp>
#include <core/common/types/unitconfig.hpp>

#include "itf/imageinfoprovider.hpp"
#include "itf/storage.hpp"

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
     */
    Instance(const InstanceInfo& info, StorageItf& storage, ImageInfoProviderItf& imageInfoProvider);

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
     * Returns monitoring data.
     *
     * @return const MonitoringData&.
     */
    const MonitoringData& GetMonitoringData() const { return mMonitoringData; }

    /**
     * Returns provider identifier.
     *
     * @return const String&.
     */
    const String& GetProviderID() const { return mProviderID; }

    /**
     * Sets provider identifier.
     *
     * @param providerID provider identifier.
     */
    void SetProviderID(const String& providerID) { mProviderID = providerID; }

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
     * Schedules instance to node.
     *
     * @param info instance information.
     * @param nodeID node identifier.
     * @return Error.
     */
    Error Schedule(const aos::InstanceInfo& info, const String& nodeID);

    /**
     * Sets error state.
     *
     * @param err error object.
     */
    void SetError(const Error& err);

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
     * @return Error.
     */
    virtual Error Cache() = 0;

    /**
     * Returns requested CPU.
     *
     * @param nodeConfig node configuration.
     * @param serviceConfig service configuration.
     * @return size_t.
     */
    virtual size_t GetRequestedCPU(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig) = 0;

    /**
     * Returns requested RAM.
     *
     * @param nodeConfig node configuration.
     * @param serviceConfig service configuration.
     * @return size_t.
     */
    virtual size_t GetRequestedRAM(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig) = 0;

    /**
     * Gets service configuration.
     *
     * @param imageID image identifier.
     * @param config service configuration.
     * @return Error.
     */
    virtual Error GetServiceConfig(const String& imageID, oci::ServiceConfig& config) = 0;

    /**
     * Gets image configuration.
     *
     * @param imageID image identifier.
     * @param config image configuration.
     * @return Error.
     */
    virtual Error GetImageConfig(const String& imageID, oci::ImageConfig& config) = 0;

    /**
     * Gets item images.
     *
     * @param imagesInfos item images info.
     * @return Error.
     */
    virtual Error GetItemImages(Array<ImageInfo>& imagesInfos) = 0;

protected:
    InstanceInfo          mInfo;
    StorageItf&           mStorage;
    ImageInfoProviderItf& mImageInfoProvider;

private:
    InstanceStatus       mStatus;
    MonitoringData       mMonitoringData;
    StaticString<cIDLen> mProviderID;

    StaticAllocator<sizeof(oci::ServiceConfig) + sizeof(StaticArray<aos::ImageInfo, cMaxNumUpdateImages>)> mAllocator;
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
     */
    ComponentInstance(const InstanceInfo& info, StorageItf& storage, ImageInfoProviderItf& imageInfoProvider);

    /**
     * Initializes component instance.
     *
     * @return Error.
     */
    Error Init() override;

    /**
     * Removes component instance.
     *
     * @return Error.
     */
    Error Remove() override;

    /**
     * Caches component instance.
     *
     * @return Error.
     */
    Error Cache() override;

    /**
     * Returns requested CPU for component instance.
     *
     * @param nodeConfig node configuration.
     * @param serviceConfig service configuration.
     * @return size_t.
     */
    size_t GetRequestedCPU(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig) override;

    /**
     * Returns requested RAM for component instance.
     *
     * @param nodeConfig node configuration.
     * @param serviceConfig service configuration.
     * @return size_t.
     */
    size_t GetRequestedRAM(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig) override;

    /**
     * Gets service configuration for component instance.
     *
     * @param imageID image identifier.
     * @param config service configuration.
     * @return Error.
     */
    Error GetServiceConfig(const String& imageID, oci::ServiceConfig& config) override;

    /**
     * Gets image configuration for component instance.
     *
     * @param imageID image identifier.
     * @param config image configuration.
     * @return Error.
     */
    Error GetImageConfig(const String& imageID, oci::ImageConfig& config) override;

    /**
     * Gets item images for component instance.
     *
     * @param imagesInfos item images info.
     * @return Error.
     */
    Error GetItemImages(Array<ImageInfo>& imagesInfos) override;
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
     * @param imageInfoProvider interface for retrieving service information from image.
     */
    ServiceInstance(const InstanceInfo& info, UIDPool& uidPool, StorageItf& storage,
        storagestate::StorageStateItf& storageState, ImageInfoProviderItf& imageInfoProvider);

    /**
     * Initializes service instance.
     *
     * @return Error.
     */
    Error Init() override;

    /**
     * Removes service instance.
     *
     * @return Error.
     */
    Error Remove() override;

    /**
     * Caches service instance.
     *
     * @return Error.
     */
    Error Cache() override;

    /**
     * Returns requested CPU for service instance.
     *
     * @param nodeConfig node configuration.
     * @param serviceConfig service configuration.
     * @return size_t.
     */
    size_t GetRequestedCPU(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig) override;

    /**
     * Returns requested RAM for service instance.
     *
     * @param nodeConfig node configuration.
     * @param serviceConfig service configuration.
     * @return size_t.
     */
    size_t GetRequestedRAM(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig) override;

    /**
     * Gets service configuration for service instance.
     *
     * @param imageID image identifier.
     * @param config service configuration.
     * @return Error.
     */
    Error GetServiceConfig(const String& imageID, oci::ServiceConfig& config) override;

    /**
     * Gets image configuration for service instance.
     *
     * @param imageID image identifier.
     * @param config image configuration.
     * @return Error.
     */
    Error GetImageConfig(const String& imageID, oci::ImageConfig& config) override;

    /**
     * Gets item images for service instance.
     *
     * @param imagesInfos item images info.
     * @return Error.
     */
    Error GetItemImages(Array<ImageInfo>& imagesInfos) override;

private:
    static constexpr auto cDefaultResourceRation = 50.0;

    size_t ClampResource(size_t value, const Optional<size_t>& quota);
    size_t GetReqCPUFromNodeConfig(const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios);
    size_t GetReqRAMFromNodeConfig(const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios);

    UIDPool&                       mUIDPool;
    storagestate::StorageStateItf& mStorageState;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
