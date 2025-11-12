/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_NETWORKMANAGER_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_NETWORKMANAGER_HPP_

#include <core/common/tools/memory.hpp>
#include <core/common/types/network.hpp>
#include <core/sm/config.hpp>

#include "interfacefactory.hpp"
#include "interfacemanager.hpp"
#include "namespacemanager.hpp"
#include "storage.hpp"
#include "trafficmonitor.hpp"

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
 * Instance network parameters.
 */
struct InstanceNetworkParameters {
    InstanceIdent                                                   mInstanceIdent;
    aos::InstanceNetworkParameters                                  mNetworkParameters;
    StaticString<cHostNameLen>                                      mHostname;
    StaticArray<StaticString<cHostNameLen>, cMaxNumAliases>         mAliases;
    uint64_t                                                        mIngressKbit {};
    uint64_t                                                        mEgressKbit {};
    StaticArray<StaticString<cExposedPortLen>, cMaxNumExposedPorts> mExposedPorts;
    StaticArray<Host, cMaxNumHosts>                                 mHosts;
    StaticString<cFilePathLen>                                      mHostsFilePath;
    StaticString<cFilePathLen>                                      mResolvConfFilePath;
    uint64_t                                                        mUploadLimit {};
    uint64_t                                                        mDownloadLimit {};

    /**
     * Compares network parameters.
     *
     * @param instanceNetworkParams instance network parameters to compare.
     * @return bool.
     */
    bool operator==(const InstanceNetworkParameters& instanceNetworkParams) const
    {
        return mInstanceIdent == instanceNetworkParams.mInstanceIdent
            && mNetworkParameters == instanceNetworkParams.mNetworkParameters
            && mHostname == instanceNetworkParams.mHostname && mAliases == instanceNetworkParams.mAliases
            && mIngressKbit == instanceNetworkParams.mIngressKbit && mEgressKbit == instanceNetworkParams.mEgressKbit
            && mExposedPorts == instanceNetworkParams.mExposedPorts && mHosts == instanceNetworkParams.mHosts
            && mHostsFilePath == instanceNetworkParams.mHostsFilePath
            && mResolvConfFilePath == instanceNetworkParams.mResolvConfFilePath
            && mUploadLimit == instanceNetworkParams.mUploadLimit
            && mDownloadLimit == instanceNetworkParams.mDownloadLimit;
    }

    /**
     * Compares instance network parameters.
     *
     * @param instanceNetworkParameters instance network parameters to compare.
     * @return bool.
     */
    bool operator!=(const InstanceNetworkParameters& instanceNetworkParameters) const
    {
        return !operator==(instanceNetworkParameters);
    }
};

/**
 * Network manager interface.
 */
class NetworkManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NetworkManagerItf() = default;

    /**
     * Returns instance's network namespace path.
     *
     * @param instanceID instance id.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> GetNetnsPath(const String& instanceID) const = 0;

    /**
     * Updates networks.
     *
     * @param networks network parameters.
     * @return Error.
     */
    virtual Error UpdateNetworks(const Array<aos::NetworkParameters>& networks) = 0;

    /**
     * Adds instance to network.
     *
     * @param instanceID instance id.
     * @param networkID network id.
     * @param instanceNetworkParameters instance network parameters.
     * @return Error.
     */
    virtual Error AddInstanceToNetwork(
        const String& instanceID, const String& networkID, const InstanceNetworkParameters& instanceNetworkParameters)
        = 0;

    /**
     * Removes instance from network.
     *
     * @param instanceID instance id.
     * @param networkID network id.
     * @return Error.
     */
    virtual Error RemoveInstanceFromNetwork(const String& instanceID, const String& networkID) = 0;

    /**
     * Returns instance's IP address.
     *
     * @param instanceID instance id.
     * @param networkID network id.
     * @param[out] ip instance's IP address.
     * @return Error.
     */
    virtual Error GetInstanceIP(const String& instanceID, const String& networkID, String& ip) const = 0;

    /**
     * Returns instance's traffic.
     *
     * @param instanceID instance id.
     * @param[out] inputTraffic instance's input traffic.
     * @param[out] outputTraffic instance's output traffic.
     * @return Error.
     */
    virtual Error GetInstanceTraffic(const String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic) const
        = 0;

    /**
     * Gets system traffic.
     *
     * @param[out] inputTraffic system input traffic.
     * @param[out] outputTraffic system output traffic.
     * @return Error.
     */
    virtual Error GetSystemTraffic(uint64_t& inputTraffic, uint64_t& outputTraffic) const = 0;

    /**
     * Sets the traffic period.
     *
     * @param period traffic period.
     * @return Error
     */
    virtual Error SetTrafficPeriod(TrafficPeriod period) = 0;
};

/**
 * Link attributes.
 */
struct LinkAttrs;

/**
 * Link interface.
 */
class LinkItf {
public:
    /**
     * Destructor.
     */
    virtual ~LinkItf() = default;

    /**
     * Gets link attributes.
     *
     * @return Link attributes.
     */
    virtual const LinkAttrs& GetAttrs() const = 0;

    /**
     * Gets link type.
     *
     * @return Link type.
     */
    virtual const char* GetType() const = 0;
};

/**
 * Address list.
 */
struct IPAddr;

/**
 * Route info.
 */
struct RouteInfo;

/** @}*/

} // namespace aos::sm::networkmanager

#endif
