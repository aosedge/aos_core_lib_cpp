/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_NETWORKMANAGER_HPP_
#define AOS_NETWORKMANAGER_HPP_

#include "aos/common/types.hpp"
#include "aos/sm/config.hpp"

namespace aos {
namespace sm {
namespace networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Max number of network manager aliases.
 */
static constexpr auto cMaxNumAliases = AOS_CONFIG_NETWORKMANAGER_MAX_NUM_ALIASES;

/**
 * Max number of network manager exposed ports.
 */
static constexpr auto cMaxNumExposedPorts = AOS_CONFIG_NETWORKMANAGER_MAX_NUM_EXPOSED_PORTS;

/**
 * Network parameters set for service provider.
 */
struct NetworkParameters {
    StaticString<cHostNameLen> mNetworkID;
    StaticString<cIPLen>       mSubnet;
    StaticString<cIPLen>       mIP;
    uint64_t                   mVlanID;
    StaticString<cHostNameLen> mVlanIfName;

    /**
     * Compares network parameters.
     *
     * @param networkParams network parameters to compare.
     * @return bool.
     */
    bool operator==(const NetworkParameters& networkParams) const
    {
        return mNetworkID == networkParams.mNetworkID && mSubnet == networkParams.mSubnet && mIP == networkParams.mIP
            && mVlanID == networkParams.mVlanID && mVlanIfName == networkParams.mVlanIfName;
    }

    /**
     * Compares network parameters.
     *
     * @param networkParams network parameters to compare.
     * @return bool.
     */
    bool operator!=(const NetworkParameters& networkParams) const { return !operator==(networkParams); }
};

/**
 * Network parameters set for instance.
 */
struct NetworkParams {
    InstanceIdent                                              mInstanceIdent;
    aos::NetworkParameters                                     mNetworkParameters;
    StaticString<cHostNameLen>                                 mHostname;
    StaticArray<StaticString<cHostNameLen>, cMaxNumAliases>    mAliases;
    uint64_t                                                   mIngressKbit;
    uint64_t                                                   mEgressKbit;
    StaticArray<StaticString<cPortLen>, cMaxNumExposedPorts>   mExposedPorts;
    StaticArray<Host, cMaxNumHosts>                            mHosts;
    StaticArray<StaticString<cHostNameLen>, cMaxNumDNSServers> mDNSSevers;
    StaticString<cFilePathLen>                                 mHostsFilePath;
    StaticString<cFilePathLen>                                 mResolvConfFilePath;
    uint64_t                                                   mUploadLimit;
    uint64_t                                                   mDownloadLimit;

    /**
     * Compares network parameters.
     *
     * @param networkParams network parameters to compare.
     * @return bool.
     */
    bool operator==(const NetworkParams& networkParams) const
    {
        return mInstanceIdent == networkParams.mInstanceIdent && mNetworkParameters == networkParams.mNetworkParameters
            && mHostname == networkParams.mHostname && mAliases == networkParams.mAliases
            && mIngressKbit == networkParams.mIngressKbit && mEgressKbit == networkParams.mEgressKbit
            && mExposedPorts == networkParams.mExposedPorts && mHosts == networkParams.mHosts
            && mDNSSevers == networkParams.mDNSSevers && mHostsFilePath == networkParams.mHostsFilePath
            && mResolvConfFilePath == networkParams.mResolvConfFilePath && mUploadLimit == networkParams.mUploadLimit
            && mDownloadLimit == networkParams.mDownloadLimit;
    }

    /**
     * Compares network parameters.
     *
     * @param networkParams network parameters to compare.
     * @return bool.
     */
    bool operator!=(const NetworkParams& networkParams) const { return !operator==(networkParams); }
};

/**
 * Network manager storage interface.
 */
class StorageItf {
public:
    /**
     * Removes network info from storage.
     *
     * @param networkID network ID to remove.
     * @return Error.
     */
    virtual Error RemoveNetworkInfo(const String& networkID) = 0;

    /**
     * Adds network info to storage.
     *
     * @param info network info.
     * @return Error.
     */
    virtual Error AddNetworkInfo(const NetworkParameters& info) = 0;

    /**
     * Returns network information.
     *
     * @param networks[out] network information result.
     * @return Error.
     */
    virtual Error GetNetworksInfo(Array<NetworkParameters>& networks) const = 0;

    /**
     * Sets traffic monitor data.
     *
     * @param chain chain.
     * @param time time.
     * @param value value.
     * @return Error.
     */
    virtual Error SetTrafficMonitorData(const String& chain, const Time& time, uint64_t value) = 0;

    /**
     * Returns traffic monitor data.
     *
     * @param chain chain.
     * @param time[out] time.
     * @param value[out] value.
     * @return Error.
     */
    virtual Error GetTrafficMonitorData(const String& chain, Time& time, uint64_t& value) const = 0;

    /**
     * Removes traffic monitor data.
     *
     * @param chain chain.
     * @return Error.
     */
    virtual Error RemoveTrafficMonitorData(const String& chain) = 0;

    /**
     * Destroys storage interface.
     */
    virtual ~StorageItf() = default;
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
     * @param[out] netnsPath instance's network namespace path.
     * @return Error.
     */
    virtual Error GetNetnsPath(const String& instanceID, String& netnsPath) const = 0;

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
     * @param network network parameters.
     * @return Error.
     */
    virtual Error AddInstanceToNetwork(const String& instanceID, const String& networkID, const NetworkParams& network)
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
     * Sets the traffic period.
     *
     * @param period traffic period.
     * @return Error
     */
    virtual Error SetTrafficPeriod(uint32_t period) = 0;
};

/** @}*/

} // namespace networkmanager
} // namespace sm
} // namespace aos

#endif
