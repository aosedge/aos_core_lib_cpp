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

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
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
     * @return Error.
     */
    Error Init(const Config& config, imagemanager::ItemInfoProviderItf& itemInfoProvider,
        storagestate::StorageStateItf& storageState, oci::OCISpecItf& ociSpec, IdentifierPoolValidator gidValidator,
        IdentifierPoolValidator uidValidator, StorageItf& storage);

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
     * Returns the collection of running instances.
     *
     * @return Array<InstanceStatus>&.
     */
    Array<InstanceStatus>& GetRunningInstances();

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
     * Creates new instance object.
     *
     * @param request run instance request.
     * @param index index of the instance.
     * @return RetWithError<SharedPtr<Instance>>.
     */
    RetWithError<SharedPtr<Instance>> CreateInstance(const RunInstanceRequest& request, uint64_t index);

    /**
     * Creates new instance object.
     *
     * @param request run instance request.
     * @param nodeID bound node ID.
     * @param newInstances array of new instances.
     * @return RetWithError<SharedPtr<Instance>>.
     */
    RetWithError<SharedPtr<Instance>> CreateInstance(
        const RunInstanceRequest& request, const String& nodeID, const Array<SharedPtr<Instance>>& newInstances);

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

    /**
     * Checks if instance is subject enabled.
     * @param instance instance.
     * @return bool.
     */
    bool IsSubjectEnabled(const Instance& instance);

    /**
     * Checks if instance is scheduled.
     *
     * @param id instance identifier.
     * @return bool.
     */
    bool IsScheduled(const InstanceIdent& id);

    /**
     * Updates running instances.
     *
     * @param statuses list of running instance statuses.
     * @return Error.
     */
    Error UpdateRunningInstances(const String& nodeID, const Array<InstanceStatus>& statuses);

    /**
     * Schedules instances on specified node and runtime.
     *
     * @param instance instance.
     * @param node node.
     * @param runtimeID runtime ID.
     * @return Error.
     */
    Error ScheduleInstance(SharedPtr<Instance>& instance, NodeItf& node, const String& runtimeID);

    /**
     * Adds instances with specified error to schedule list.
     * It will not be sent for execution but will be available for future rebalancing.
     *
     * @param instance instance.
     * @param error error.
     * @return Error.
     */
    Error ScheduleInstance(SharedPtr<Instance>& instance, const Error& error);

    /**
     * Overrides environment variables.
     *
     * @param envVars environment variables.
     * @return bool true if env vars changed, false otherwise.
     */
    bool OverrideEnvVars(const OverrideEnvVarsRequest& envVars);

private:
    static constexpr auto cRemovePeriod  = Time::cDay;
    static constexpr auto cAllocatorSize = Max(sizeof(ComponentInstance), sizeof(ServiceInstance)) * cMaxNumInstances
        + sizeof(InstanceInfo) * cMaxNumInstances + sizeof(InstanceInfo) + sizeof(oci::ImageIndex);
    static constexpr auto cInstanceAllocatorSize = sizeof(oci::ImageConfig) + sizeof(oci::ItemConfig)
        + sizeof(InstanceStatus) + sizeof(oci::ImageIndex) + sizeof(EnvVarArray);

    Error LoadInstancesFromStorage();
    Error LoadInstanceFromStorage(const InstanceInfo& info);
    Error LoadInstanceStatuses();

    Error SetExpiredStatus();
    Error RemoveOutdatedInstances();
    Error ClearInstancesWithDeletedImages();

    RetWithError<SharedPtr<Instance>> CreateInstance(const InstanceInfo& info);

    SharedPtr<Instance> FindReadyInstance(const InstanceIdent& id);
    SharedPtr<Instance> FindReadyInstance(const String& itemID, const String& subjectID, const String& nodeID);
    uint64_t            FindIndexForNewInstance(const String& itemID, const String& subjectID);

    UniquePtr<InstanceInfo> CreateInfo(
        const InstanceIdent& id, const String& nodeID, const RunInstanceRequest& request);

    SharedPtr<Instance> FindInstance(const Array<SharedPtr<Instance>>& instances, const InstanceIdent& id);
    SharedPtr<Instance> FindInstance(const Array<SharedPtr<Instance>>& instances, const String& itemID,
        const String& subjectID, const String& nodeID);
    uint64_t            FindIndexForNewInstance(
                   const Array<SharedPtr<Instance>>& instances, const String& itemID, const String& subjectID);

    Config      mConfig;
    StorageItf* mStorage {};

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

    StaticArray<InstanceStatus, cMaxNumInstances> mPreinstalledComponents;
    StaticArray<InstanceStatus, cMaxNumInstances> mRunningInstances;

    SubjectArray           mSubjects;
    OverrideEnvVarsRequest mEnvVarsOverrides;
};

/**
 * Pushes a value to an array if it is not already present.
 *
 * @tparam T type of the value to push.
 * @param array array to push the value to.
 * @param value value to push.
 * @return Error.
 */
template <typename Container, typename T>
Error PushUnique(Container& array, const T& value)
{
    if (array.Contains(value)) {
        return ErrorEnum::eNone;
    }

    return array.PushBack(value);
}

/** @}*/

} // namespace aos::cm::launcher

#endif
