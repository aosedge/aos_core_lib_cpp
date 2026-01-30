/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_INSTANCEMANAGER_HPP_
#define AOS_CORE_CM_LAUNCHER_INSTANCEMANAGER_HPP_

#include <core/cm/imagemanager/itf/blobinfoprovider.hpp>
#include <core/cm/imagemanager/itf/iteminfoprovider.hpp>
#include <core/cm/storagestate/storagestate.hpp>
#include <core/common/instancestatusprovider/itf/instancestatusprovider.hpp>
#include <core/common/monitoring/itf/monitoringdata.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/timer.hpp>

#include "itf/launcher.hpp"

#include "config.hpp"
#include "instance.hpp"
#include "networkmanager.hpp"

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Auxiliary class accumulating data about running service instances.
 */
class InstanceManager {
public:
    /**
     * Initializes the instance manager with configuration and required interfaces.
     *
     * @param config Configuration object.
     * @param imageInfoProvider Interface for retrieving service information from images.
     * @param storageState Interface for managing storage and state partitions.
     * @param gidValidator GID validator.
     * @param uidValidator UID validator.
     * @param storage Interface to persistent storage.
     * @param networkManager Interface for managing networks of service instances.
     * @return Error.
     */
    Error Init(const Config& config, imagemanager::ItemInfoProviderItf& itemInfoProvider,
        storagestate::StorageStateItf& storageState, oci::OCISpecItf& ociSpec, IdentifierPoolValidator gidValidator,
        IdentifierPoolValidator uidValidator, StorageItf& storage, NetworkManager& networkManager);

    /**
     * Starts the instance manager.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops the instance manager.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Prepares instance manager for balancing.
     *
     * @return Error.
     */
    Error PrepareForBalancing();

    /**
     * Returns the collection of currently active instances.
     *
     * @return Array<SharedPtr<Instance>>&.
     */
    Array<SharedPtr<Instance>>& GetActiveInstances();

    /**
     * Returns the collection of scheduled instances.
     *
     * @return Array<SharedPtr<Instance>>&.
     */
    Array<SharedPtr<Instance>>& GetScheduledInstances();

    /**
     * Returns the collection of currently cached instances.
     *
     * @return Array<SharedPtr<Instance>>&.
     */
    Array<SharedPtr<Instance>>& GetCachedInstances();

    /**
     * Returns the collection of preinstalled components.
     *
     * @return Array<InstanceStatus>&.
     */
    Array<InstanceStatus>& GetPreinstalledComponents();

    /**
     * Finds an active instance by its identifier.
     *
     * @param id instance identifier.
     * @return SharedPtr<Instance>.
     */
    SharedPtr<Instance> FindActiveInstance(const InstanceIdent& id);

    /**
     * Finds a scheduled instance by its identifier.
     *
     * @param id instance identifier.
     * @return SharedPtr<Instance>.
     */
    SharedPtr<Instance> FindScheduledInstance(const InstanceIdent& id);

    /**
     * Finds a cached instance by its identifier.
     *
     * @param id instance identifier.
     * @return SharedPtr<Instance>.
     */
    SharedPtr<Instance> FindCachedInstance(const InstanceIdent& id);

    /**
     * Finds a preinstalled component by its identifier.
     *
     * @param id instance identifier.
     * @return InstanceStatus*.
     */
    InstanceStatus* FindPreinstalledComponent(const InstanceIdent& id);

    /**
     * Updates the status of a managed instance.
     *
     * @param status new instance status.
     * @return Error.
     */
    Error UpdateStatus(const InstanceStatus& status);

    /**
     * Schedules instance.
     *
     * @param id instance identifier.
     * @param request run instance request.
     * @return Error.
     */
    Error ScheduleInstance(const InstanceIdent& id, const RunInstanceRequest& request);

    /**
     * Schedules instance.
     *
     * @param instance instance.
     * @return Error.
     */
    Error ScheduleInstance(SharedPtr<Instance>& instance);

    /**
     * Submits all stashed instances for execution.
     *
     * @return Error.
     */
    Error SubmitScheduledInstances();

    /**
     * Disables instance.
     *
     * @param instance instance.
     * @return Error.
     */
    Error DisableInstance(SharedPtr<Instance>& instance);

    /**
     * Updates monitoring data for active instances.
     *
     * @param monitoringData array of monitoring data for instances.
     */
    void UpdateMonitoringData(const Array<monitoring::InstanceMonitoringData>& monitoringData);

    /**
     * Saves subjects and returns flag indicating whether rebalancing is required.
     *
     * @param subjects subjects.
     * @return RetWithError<bool>.
     */
    RetWithError<bool> SetSubjects(const Array<StaticString<cIDLen>>& subjects);

private:
    static constexpr auto cRemovePeriod  = Time::cDay;
    static constexpr auto cAllocatorSize = Max(sizeof(ComponentInstance), sizeof(ServiceInstance)) * cMaxNumInstances
        + sizeof(InstanceInfo) * cMaxNumInstances + sizeof(InstanceInfo) + sizeof(oci::ImageIndex);
    static constexpr auto cInstanceAllocatorSize
        = Max(sizeof(oci::ImageConfig) + sizeof(oci::ItemConfig), sizeof(oci::ImageIndex));

    Error LoadInstancesFromStorage();
    Error LoadInstanceFromStorage(const InstanceInfo& info);

    Error SetExpiredStatus();
    Error RemoveOutdatedInstances();
    Error ClearInstancesWithDeletedImages();

    RetWithError<SharedPtr<Instance>> CreateInstance(const InstanceInfo& info);

    Error CheckSubjectEnabled(SharedPtr<Instance>& instance);
    bool  IsSubjectEnabled(const Instance& instance);

    SharedPtr<Instance> ScheduleReadyInstance(const InstanceIdent& id);
    void                CreateInfo(const InstanceIdent& id, const RunInstanceRequest& request, InstanceInfo& info);

    Config          mConfig;
    StorageItf*     mStorage {};
    NetworkManager* mNetworkManager {};

    ImageInfoProvider mImageInfoProvider;
    StorageState      mStorageState;
    UIDPool           mUIDPool;
    GIDPool           mGIDPool;

    Timer mCleanInstancesTimer;
    Timer mInitTimer;

    StaticAllocator<cAllocatorSize, cMaxNumInstances> mAllocator;
    StaticAllocator<cInstanceAllocatorSize>           mInstanceAllocator;

    StaticArray<SharedPtr<Instance>, cMaxNumInstances> mActiveInstances;
    StaticArray<SharedPtr<Instance>, cMaxNumInstances> mScheduledInstances;
    StaticArray<SharedPtr<Instance>, cMaxNumInstances> mCachedInstances;
    StaticArray<InstanceStatus, cMaxNumInstances>      mPreinstalledComponents;

    SubjectArray mSubjects;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
