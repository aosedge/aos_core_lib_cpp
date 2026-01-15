/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_ITF_STORAGE_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_STORAGE_HPP_

#include <core/common/monitoring/itf/instanceparamsprovider.hpp>
#include <core/common/types/instance.hpp>

namespace aos::sm::launcher {

/**
 * Launcher storage interface.
 */
class StorageItf {
public:
    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;

    /**
     * Gets all instances infos from storage.
     *
     * @param[out] infos array to store instance infos.
     * @return Error.
     */
    virtual Error GetAllInstancesInfos(Array<InstanceInfo>& infos) = 0;

    /**
     * Updates instance info in storage. Inserts a new record
     * if it does not exist.
     *
     * @param info instance info to store.
     * @return Error.
     */
    virtual Error UpdateInstanceInfo(const InstanceInfo& info) = 0;

    /**
     * Deletes instance info from storage.
     *
     * @param instanceIdent instance ident.
     * @return Error.
     */
    virtual Error RemoveInstanceInfo(const InstanceIdent& ident) = 0;
};

/** @}*/

} // namespace aos::sm::launcher

#endif
