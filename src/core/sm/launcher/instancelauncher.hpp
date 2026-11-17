/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_INSTANCELAUNCHER_HPP_
#define AOS_CORE_SM_LAUNCHER_INSTANCELAUNCHER_HPP_

#include <core/common/imagemanager/itf/iteminfoprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/array.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/string.hpp>
#include <core/sm/networkmanager/itf/networkmanager.hpp>
#include <core/sm/resourcemanager/itf/resourceinfoprovider.hpp>

#include "instancenetwork.hpp"
#include "instanceregistry.hpp"
#include "itemconfigloader.hpp"
#include "itf/storage.hpp"
#include "runtimemanager.hpp"

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Performs the steps which start and stop a node instance: prepares the instance, starts and stops its network and
 * starts and stops it in its runtime. The instance launcher doesn't change the instance state: it returns the error of
 * the step, and the caller decides about the instance.
 */
class InstanceLauncher {
public:
    /**
     * Initializes instance launcher.
     *
     * @param allocator allocator to use for temporary objects.
     * @param runtimes node runtimes.
     * @param storage storage.
     * @param itemInfoProvider item info provider.
     * @param ociSpec OCI spec.
     * @param networkManager network manager.
     * @param resourceInfoProvider resource info provider.
     */
    void Init(AllocatorItf& allocator, RuntimeManager& runtimes, StorageItf& storage,
        common::imagemanager::ItemInfoProviderItf& itemInfoProvider, oci::OCISpecItf& ociSpec,
        networkmanager::NetworkManagerItf&        networkManager,
        resourcemanager::ResourceInfoProviderItf& resourceInfoProvider);

    /**
     * Loads the item configs of the instance and sets its offline TTL.
     *
     * @param instance instance data.
     * @return Error.
     */
    Error LoadConfigs(InstanceData& instance);

    /**
     * Prepares the instance: stores it, and, for a service, loads its item configs and creates its network.
     *
     * @param instance instance data.
     * @return Error.
     */
    Error Prepare(InstanceData& instance);

    /**
     * Starts the instance network.
     *
     * @param instance instance data.
     * @return Error.
     */
    Error StartNetwork(InstanceData& instance);

    /**
     * Starts the instance in its runtime. The configs of a service which weren't loaded on module start are loaded
     * first: a broken content fails the start with eIntegrity. An already started instance isn't an error.
     *
     * @param instance instance data.
     * @return Error.
     */
    Error Start(InstanceData& instance);

    /**
     * Stops the instance in its runtime. A not found instance isn't an error.
     *
     * @param instance instance data.
     * @return Error.
     */
    Error Stop(InstanceData& instance);

    /**
     * Stops the instance network.
     *
     * @param instance instance data.
     * @return Error.
     */
    Error StopNetwork(InstanceData& instance);

    /**
     * Releases the instance: removes it from the storage and releases its network.
     *
     * @param instance instance data.
     * @return Error.
     */
    Error Release(const InstanceData& instance);

    /**
     * Begins the batch of the network starts and stops.
     *
     * @return Error.
     */
    Error BeginNetworkBatch();

    /**
     * Applies the batch of the network starts and stops.
     *
     * @param[out] failedInstanceIDs IDs of the instances whose networks failed to apply.
     * @return Error.
     */
    Error FlushNetworkBatch(Array<StaticString<cIDLen>>& failedInstanceIDs);

private:
    Error LoadConfigs(InstanceData& instance, oci::ItemConfig& itemConfig, oci::ImageConfig& imageConfig);

    AllocatorItf*    mAllocator {};
    RuntimeManager*  mRuntimes {};
    StorageItf*      mStorage {};
    ItemConfigLoader mItemConfigLoader;
    InstanceNetwork  mNetwork;
};

/** @}*/

} // namespace aos::sm::launcher

#endif
