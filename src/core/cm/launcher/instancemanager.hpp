/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_INSTANCEMANAGER_HPP_
#define AOS_CORE_CM_LAUNCHER_INSTANCEMANAGER_HPP_

#include <core/cm/storagestate/storagestate.hpp>

#include <core/common/tools/timer.hpp>

#include "itf/instancestatusprovider.hpp"
#include "itf/instancestatusreceiver.hpp"
#include "itf/launcher.hpp"

#include "instance.hpp"

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Launcher configuration.
 */
struct Config {
    Duration mNodesConnectionTimeout;
    Duration mServiceTTL;
};

/**
 * Auxiliary class accumulating data about running service instances.
 */
class InstanceManager {
public:
    /**
     * Initializes the instance manager with configuration and required interfaces.
     *
     * @param config Configuration object.
     * @param storage Interface to persistent storage.
     * @param imageInfoProvider Interface for retrieving service information from images.
     * @param storageState Interface for managing storage and state partitions.
     */
    void Init(const Config& config, StorageItf& storage, ImageInfoProviderItf& imageInfoProvider,
        storagestate::StorageStateItf& storageState);

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
     * Returns the collection of currently active instances.
     *
     * @return Array<SharedPtr<Instance>>&.
     */
    Array<SharedPtr<Instance>>& GetActiveInstances();

    /**
     * Finds an active instance by its identifier.
     *
     * @param id instance identifier.
     * @return SharedPtr<Instance>*.
     */
    SharedPtr<Instance>* FindActiveInstance(const InstanceIdent& id);

    /**
     * Updates the status of a managed instance.
     *
     * @param status new instance status.
     * @return Error.
     */
    Error UpdateStatus(const InstanceStatus& status);

    /**
     * Adds a new instance to the manager's stash for later submission.
     *
     * @param id instance identifier.
     * @param request run request details.
     * @return Error.
     */
    Error AddInstanceToStash(const InstanceIdent& id, const RunInstanceRequest& request);

    /**
     * Returns the collection of stashed instances.
     *
     * @return Array<SharedPtr<Instance>>&.
     */
    Array<SharedPtr<Instance>>& GetStashInstances();

    /**
     * Finds a stashed instance by its identifier.
     *
     * @param id instance identifier.
     * @return SharedPtr<Instance>*.
     */
    SharedPtr<Instance>* FindStashInstance(const InstanceIdent& id);

    /**
     * Submits all stashed instances for execution.
     *
     * @return Error.
     */
    Error SubmitStash();

    /**
     * Updates monitoring data for active instances.
     *
     * @param monitoringData array of monitoring data for instances.
     */
    void UpdateMonitoringData(const Array<monitoring::InstanceMonitoringData>& monitoringData);

private:
    Error LoadInstancesFromStorage();
    Error LoadInstanceFromStorage(const InstanceInfo& info);

    Error SetExpiredStatus();
    Error RemoveOutdatedInstances();
    Error ClearInstancesWithDeletedImages();

    RetWithError<SharedPtr<Instance>> CreateInstance(const InstanceInfo& info);

    static constexpr auto cRemovePeriod = Time::cDay;

    Config                         mConfig;
    StorageItf*                    mStorage           = nullptr;
    storagestate::StorageStateItf* mStorageState      = nullptr;
    ImageInfoProviderItf*          mImageInfoProvider = nullptr;

    Timer   mCleanInstancesTimer;
    Timer   mInitTimer;
    UIDPool mUIDPool;

    StaticAllocator<Max(sizeof(ComponentInstance), sizeof(ServiceInstance)) * cMaxNumInstances
            + sizeof(InstanceInfo) * cMaxNumInstances + sizeof(InstanceInfo),
        cMaxNumInstances>
        mAllocator;

    StaticArray<SharedPtr<Instance>, cMaxNumInstances> mActiveInstances;
    StaticArray<SharedPtr<Instance>, cMaxNumInstances> mStashInstances;
    StaticArray<SharedPtr<Instance>, cMaxNumInstances> mCachedInstances;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
