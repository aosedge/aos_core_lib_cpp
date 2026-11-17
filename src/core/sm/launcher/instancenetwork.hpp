/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_INSTANCENETWORK_HPP_
#define AOS_CORE_SM_LAUNCHER_INSTANCENETWORK_HPP_

#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/array.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/string.hpp>
#include <core/sm/networkmanager/itf/networkmanager.hpp>
#include <core/sm/resourcemanager/itf/resourceinfoprovider.hpp>

#include "instanceregistry.hpp"

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Manages the networks of the node instances.
 */
class InstanceNetwork {
public:
    /**
     * Initializes instance network.
     *
     * @param allocator allocator to use for temporary objects.
     * @param networkManager network manager.
     * @param resourceInfoProvider resource info provider.
     */
    void Init(AllocatorItf& allocator, networkmanager::NetworkManagerItf& networkManager,
        resourcemanager::ResourceInfoProviderItf& resourceInfoProvider);

    /**
     * Creates the instance network configured by the item and image configs. An existing network isn't an error.
     *
     * @param instance instance data.
     * @param itemConfig item config.
     * @param imageConfig image config.
     * @return Error.
     */
    Error Create(const InstanceData& instance, const oci::ItemConfig& itemConfig, const oci::ImageConfig& imageConfig);

    /**
     * Starts the instance network. An already started network isn't an error.
     *
     * @param instance instance data.
     * @return Error.
     */
    Error Start(const InstanceData& instance);

    /**
     * Stops the instance network. A not found network isn't an error.
     *
     * @param instance instance data.
     * @return Error.
     */
    Error Stop(const InstanceData& instance);

    /**
     * Releases the instance network.
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
    Error BeginBatch();

    /**
     * Applies the batch of the network starts and stops.
     *
     * @param[out] failedInstanceIDs IDs of the instances whose networks failed to apply.
     * @return Error.
     */
    Error FlushBatch(Array<StaticString<cIDLen>>& failedInstanceIDs);

    /**
     * Returns the network config of the instance.
     *
     * @param instance instance info.
     * @param itemConfig item config.
     * @param imageConfig image config.
     * @param[out] networkConfig instance network config.
     * @return Error.
     */
    Error GetNetworkConfig(const InstanceInfo& instance, const oci::ItemConfig& itemConfig,
        const oci::ImageConfig& imageConfig, networkmanager::InstanceNetworkConfig& networkConfig);

private:
    AllocatorItf*                             mAllocator {};
    networkmanager::NetworkManagerItf*        mNetworkManager {};
    resourcemanager::ResourceInfoProviderItf* mResourceInfoProvider {};
};

/** @}*/

} // namespace aos::sm::launcher

#endif
