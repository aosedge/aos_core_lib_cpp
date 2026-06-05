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
    StaticArray<StaticString<cDeviceNameLen>, cMaxNumHostDevices> mDevices;
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
    virtual Error GetResourcesInfos(Array<aos::ResourceInfo>& resources) = 0;

    /**
     * Returns resource info by name.
     *
     * @param name resource name.
     * @param[out] resourceInfo resource info.
     * @return Error.
     */
    virtual Error GetResourceInfo(const String& name, ResourceInfo& resourceInfo) = 0;
};

/** @}*/

} // namespace aos::sm::resourcemanager

#endif
