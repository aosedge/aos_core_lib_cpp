/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_RESOURCEMANAGER_ITF_RESOURCEINFOPROVIDER_HPP_
#define AOS_CORE_SM_RESOURCEMANAGER_ITF_RESOURCEINFOPROVIDER_HPP_

#include <core/common/types/common.hpp>
#include <core/common/types/envvars.hpp>
#include <core/common/types/network.hpp>

namespace aos::sm::resourcemanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Resource info structure.
 */
struct ResourceInfo : public aos::ResourceInfo {
    StaticArray<StaticString<cGroupNameLen>, cMaxNumGroups>       mGroups;
    StaticArray<Mount, cMaxNumFSMounts>                           mMounts;
    StaticArray<StaticString<cEnvVarLen>, cMaxNumEnvVariables>    mEnv;
    StaticArray<Host, cMaxNumHosts>                               mHosts;
    StaticArray<StaticString<cDeviceNameLen>, cMaxNumHostDevices> mHostDevices;
};

/**
 * Resource info provider interface.
 */
class ResourceInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ResourceInfoProviderItf() = default;

    /**
     * Returns resources info.
     *
     * @param[out] resources resources info.
     * @return Error.
     */
    virtual Error GetResourcesInfos(Array<ResourceInfo>& resources) = 0;
};

/** @}*/

} // namespace aos::sm::resourcemanager

#endif
