/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_RUNREQUESTSLOADER_HPP_
#define AOS_CORE_CM_LAUNCHER_RUNREQUESTSLOADER_HPP_

#include "instancemanager.hpp"
#include "itf/storage.hpp"
#include "node.hpp"

namespace aos::cm::launcher {

/**
 * Loads and persists RunInstanceRequest: saves on RunInstances, loads on Start.
 */
class RunRequestsLoader {
public:
    /**
     * Initializes the loader.
     *
     * @param storage storage interface.
     * @param instanceManager instance manager.
     * @param imageInfoProvider image info provider.
     */
    void Init(StorageItf& storage, InstanceManager& instanceManager, ImageInfoProvider& imageInfoProvider);

    /**
     * Saves run requests to internal buffer and to storage.
     * Call when launcher receives RunInstances.
     *
     * @param requests run requests to save.
     * @return Error.
     */
    Error Save(const Array<RunInstanceRequest>& requests);

    /**
     * Loads run requests from storage into internal buffer.
     *
     * @return Error.
     */
    Error Load();

    /**
     * Creates instances from run requests.
     *
     * @param nodes nodes to create instances on.
     * @param[out] instances instances created.
     */
    void CreateInstances(const Array<Node>& nodes, Array<SharedPtr<Instance>>& instances);

private:
    using CombinedRuntimesArray
        = StaticArray<StaticString<cRuntimeTypeLen>, oci::cMaxNumManifests * oci::cMaxNumRunners>;

    static constexpr auto cCombinedRuntimesSize = sizeof(CombinedRuntimesArray);
    static constexpr auto cAllocatorSize = sizeof(oci::ImageIndex) + cCombinedRuntimesSize + sizeof(oci::ItemConfig);

    Error GenerateInstances(
        const RunInstanceRequest& request, const Array<Node>& nodes, Array<SharedPtr<Instance>>& instances);
    Error CombinedRuntimes(
        const oci::ImageIndex& imageIndex, CombinedRuntimesArray& combinedRuntimes, oci::ItemConfig& itemConfig);
    void CreateGeneratedInstances(const RunInstanceRequest& request, const Array<Node>& nodes,
        const CombinedRuntimesArray& combinedRuntimes, Array<SharedPtr<Instance>>& instances);

    StorageItf*        mStorage {};
    InstanceManager*   mInstanceManager {};
    ImageInfoProvider* mImageInfoProvider {};

    StaticArray<RunInstanceRequest, cMaxNumInstances> mRunRequests;

    StaticAllocator<cAllocatorSize> mAllocator;
};

} // namespace aos::cm::launcher

#endif
