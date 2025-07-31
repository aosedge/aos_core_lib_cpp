/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_STORAGE_HPP_
#define AOS_CORE_CM_LAUNCHER_STORAGE_HPP_

#include <core/common/types/types.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Represents information about a service instance.
 */
struct InstanceInfo {
    /**
     * Instance identifier.
     */
    InstanceIdent mInstanceID;

    /**
     * Node identifier.
     */
    StaticString<cNodeIDLen> mNodeID;

    /**
     * Previous node identifier, it's used for node balancing.
     */
    StaticString<cNodeIDLen> mPrevNodeID;

    /**
     * User ID.
     */
    uint32_t mUID = 0;

    /**
     * Timestamp.
     */
    Time mTimestamp;

    /**
     * Instance state.
     */
    InstanceState mState;
};

/**
 * Interface for service instance storage.
 */
class StorageItf {
public:
    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;

    /**
     * Adds a new instance to the storage.
     *
     * @param info instance information.
     * @return Error.
     */
    virtual Error AddInstance(const InstanceInfo& info) = 0;

    /**
     * Updates an existing instance in the storage.
     *
     * @param info updated instance information.
     * @return Error.
     */
    virtual Error UpdateInstance(const InstanceInfo& info) = 0;

    /**
     * Removes an instance from the storage.
     *
     * @param instanceID instance identifier.
     * @return Error.
     */
    virtual Error RemoveInstance(const InstanceIdent& instanceID) = 0;

    /**
     * Get information about a stored instance.
     *
     * @param instanceID instance identifier.
     * @param[out] info instance info.
     * @return Error.
     */
    virtual Error GetInstance(const InstanceIdent& instanceID, InstanceInfo& info) const = 0;

    /**
     * Get all stored instances.
     *
     * @param[out] instances all stored instances.
     * @return Error.
     */
    virtual Error GetInstances(Array<InstanceInfo>& instances) const = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
