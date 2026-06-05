/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_STORAGESTATE_HPP_
#define AOS_CORE_CM_LAUNCHER_STORAGESTATE_HPP_

#include <core/cm/storagestate/itf/storagestate.hpp>
#include <core/common/tools/allocator.hpp>

namespace aos::cm::launcher {

/**
 * Storage state class wraps StorageStateItf and provides additional functionality for checking
 * available size of state and storage partitions during setup.
 */
class StorageState {
public:
    /**
     * Initializes storage state.
     *
     * @param storageState storage state interface.
     */
    void Init(storagestate::StorageStateItf& storageState);

    /**
     * Starts storage state.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops storage state.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Prepares storage state for balancing.
     *
     * @return Error.
     */
    Error PrepareForBalancing();

    /**
     * Cleans up storage state instance.
     *
     * @param instanceIdent instance identifier.
     * @return Error.
     */
    Error Cleanup(const InstanceIdent& instanceIdent);

    /**
     * Removes storage state instance.
     *
     * @param instanceIdent instance identifier.
     * @return Error.
     */
    Error Remove(const InstanceIdent& instanceIdent);

    /**
     * Sets up storage/state for instance.
     *
     * @param instanceIdent instance identifier.
     * @param setupParams setup parameters.
     * @param requestedStorageSize requested storage size.
     * @param requestedStateSize requested state size.
     * @param storagePath[out] storage path.
     * @param statePath[out] state path.
     * @return Error.
     */
    Error SetupStateStorage(const InstanceIdent& instanceIdent, const storagestate::SetupParams& setupParams,
        size_t requestedStorageSize, size_t requestedStateSize, String& storagePath, String& statePath);

private:
    static constexpr auto cAllocatorSize = sizeof(size_t) * 2;

    storagestate::StorageStateItf* mStorageStateManager {};

    StaticAllocator<cAllocatorSize> mAllocator;
    SharedPtr<size_t>               mAvailableState;
    SharedPtr<size_t>               mAvailableStorage;
};

} // namespace aos::cm::launcher

#endif
