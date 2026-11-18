/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STORAGE_HPP_
#define AOS_CM_LAUNCHER_STORAGE_HPP_

#include <core/common/types/envvars.hpp>

#include <core/cm/launcher/itf/types.hpp>

namespace aos::cm::launcher {

/** @addtogroup cm Communication Manager
 *  @{
 */

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
     * Loads all active instances from storage.
     *
     * @param[out] instances all stored instances.
     * @return Error.
     */
    virtual Error LoadActiveInstances(Array<InstanceInfo>& instances) const = 0;

    /**
     * Returns override environment variables.
     *
     * @param[out] envVars override environment variables.
     * @return Error.
     */
    virtual Error LoadOverrideEnvVars(OverrideEnvVarsRequest& envVars) const = 0;

    /**
     * Saves override environment variables.
     *
     * @param envVars override environment variables.
     * @return Error.
     */
    virtual Error SaveOverrideEnvVars(const OverrideEnvVarsRequest& envVars) = 0;

    /**
     * Loads all run requests from storage.
     *
     * @param requests run requests to load.
     * @return Error.
     */
    virtual Error LoadRunRequests(Array<RunInstanceRequest>& requests) const = 0;

    /**
     * Saves all run requests to storage.
     *
     * @param requests run requests to save.
     * @return Error.
     */
    virtual Error SaveRunRequests(const Array<RunInstanceRequest>& requests) = 0;
};

/** @}*/

} // namespace aos::cm::launcher

#endif
