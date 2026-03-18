/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_TYPES_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_TYPES_HPP_

#include <core/common/types/common.hpp>
#include <core/common/types/network.hpp>
#include <core/sm/config.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Max number of network manager aliases.
 */
static constexpr auto cMaxNumAliases = AOS_CONFIG_NETWORKMANAGER_MAX_NUM_ALIASES;

/**
 * Max number of hosts.
 */
static constexpr auto cMaxNumHosts = AOS_CONFIG_NETWORKMANAGER_MAX_NUM_HOSTS;

/**
 * Instance network config for Create (stored in DB).
 */
struct InstanceNetworkConfig {
    InstanceIdent                                                     mInstanceIdent;
    StaticString<cHostNameLen>                                        mHostname;
    StaticArray<StaticString<cHostNameLen>, cMaxNumAliases>           mAliases;
    uint64_t                                                          mIngressKbit {};
    uint64_t                                                          mEgressKbit {};
    StaticArray<StaticString<cExposedPortLen>, cMaxNumExposedPorts>   mExposedPorts;
    StaticArray<StaticString<cConnectionNameLen>, cMaxNumConnections> mAllowedConnections;
    StaticArray<Host, cMaxNumHosts>                                   mHosts;
    uint64_t                                                          mUploadLimit {};
    uint64_t                                                          mDownloadLimit {};

    bool operator==(const InstanceNetworkConfig& rhs) const
    {
        return mInstanceIdent == rhs.mInstanceIdent && mHostname == rhs.mHostname && mAliases == rhs.mAliases
            && mIngressKbit == rhs.mIngressKbit && mEgressKbit == rhs.mEgressKbit && mExposedPorts == rhs.mExposedPorts
            && mAllowedConnections == rhs.mAllowedConnections && mHosts == rhs.mHosts
            && mUploadLimit == rhs.mUploadLimit && mDownloadLimit == rhs.mDownloadLimit;
    }

    bool operator!=(const InstanceNetworkConfig& rhs) const { return !operator==(rhs); }
};

/**
 * Instance network runtime parameters for Start (not stored in DB).
 */
struct InstanceNetworkRuntimeParams {
    StaticString<cFilePathLen> mHostsFilePath;
    StaticString<cFilePathLen> mResolvConfFilePath;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
