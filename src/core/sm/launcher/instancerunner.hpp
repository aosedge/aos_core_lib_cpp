/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_INSTANCERUNNER_HPP_
#define AOS_CORE_SM_LAUNCHER_INSTANCERUNNER_HPP_

#include <core/common/imagemanager/itf/iteminfoprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/array.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>
#include <core/sm/networkmanager/itf/networkmanager.hpp>
#include <core/sm/resourcemanager/itf/resourceinfoprovider.hpp>

#include "instancenetwork.hpp"
#include "instanceregistry.hpp"
#include "itemconfigloader.hpp"
#include "itf/storage.hpp"
#include "launchpool.hpp"
#include "runtimemanager.hpp"

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Node instances to run or stop.
 */
using InstanceDataArray = StaticArray<InstanceData*, cMaxNumInstances>;

/**
 * Starts and stops node instances: stores and prepares them, starts and stops their networks in a batch and starts and
 * stops them in their runtimes. The instances are processed in parallel, each step sets the instance state: a failed
 * instance is skipped by the next steps.
 */
class InstanceRunner {
public:
    /**
     * Initializes instance runner.
     *
     * @param allocator allocator to use for temporary objects.
     * @param runtimes node runtimes.
     * @param storage storage.
     * @param itemInfoProvider item info provider.
     * @param ociSpec OCI spec.
     * @param networkManager network manager.
     * @param resourceInfoProvider resource info provider.
     * @param stateMutex mutex which guards the instance states read by the other threads.
     */
    void Init(AllocatorItf& allocator, RuntimeManager& runtimes, StorageItf& storage,
        common::imagemanager::ItemInfoProviderItf& itemInfoProvider, oci::OCISpecItf& ociSpec,
        networkmanager::NetworkManagerItf&        networkManager,
        resourcemanager::ResourceInfoProviderItf& resourceInfoProvider, Mutex& stateMutex);

    /**
     * Loads the item configs of the services restored on module start. The failures are logged: the configs are loaded
     * again when the instance is started.
     *
     * @param instances instances.
     */
    void LoadConfigs(const Array<InstanceData*>& instances);

    /**
     * Starts the instances which aren't failed. The not prepared instance is stored and, for a service, its configs
     * are loaded and its network is created. Then the networks of the services are started in a batch and the
     * instances are started in their runtimes. An already started instance isn't an error.
     *
     * @param instances instances.
     */
    void Start(const Array<InstanceData*>& instances);

    /**
     * Stops the instances in their runtimes and stops the networks of the services in a batch.
     *
     * @param instances instances.
     */
    void Stop(const Array<InstanceData*>& instances);

    /**
     * Releases the stopped instances: removes them from the storage and releases their networks.
     *
     * @param instances instances.
     */
    void Release(const Array<InstanceData*>& instances);

private:
    template <typename Filter, typename Task>
    void RunTasks(const Array<InstanceData*>& instances, const char* message, Filter filter, Task task);
    template <typename Filter, typename Task, typename HandleFailedIDs>
    void RunNetworkBatch(const Array<InstanceData*>& instances, const char* message, Filter filter, Task task,
        HandleFailedIDs handleFailedIDs);

    Error Prepare(InstanceData& instance);
    Error StartInstance(InstanceData& instance);
    Error StopInstance(InstanceData& instance);
    Error ReleaseInstance(const InstanceData& instance);
    Error LoadConfigs(InstanceData& instance);
    Error LoadConfigs(InstanceData& instance, oci::ItemConfig& itemConfig, oci::ImageConfig& imageConfig);
    void  SetState(InstanceData& instance, InstanceState state, const Error& error = ErrorEnum::eNone);
    void  Fail(InstanceData& instance, const char* message, const Error& error);

    AllocatorItf*    mAllocator {};
    RuntimeManager*  mRuntimes {};
    StorageItf*      mStorage {};
    Mutex*           mStateMutex {};
    ItemConfigLoader mItemConfigLoader;
    InstanceNetwork  mNetwork;
    LaunchPool       mPool;
};

/** @}*/

} // namespace aos::sm::launcher

#endif
