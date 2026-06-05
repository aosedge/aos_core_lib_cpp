/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_STORAGE_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_STORAGE_HPP_

#include <core/common/types/common.hpp>
#include <core/common/types/network.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Network information.
 */
struct NetworkInfo {
    StaticString<cHostNameLen>  mNetworkID;
    StaticString<cIPLen>        mSubnet;
    StaticString<cIPLen>        mIP;
    uint64_t                    mVlanID {};
    StaticString<cInterfaceLen> mVlanIfName;
    StaticString<cInterfaceLen> mBridgeIfName;

    /**
     * Default constructor.
     */
    NetworkInfo() = default;

    /**
     * Constructor.
     *
     * @param networkID network ID.
     * @param subnet subnet.
     * @param ip IP address.
     * @param vlanID VLAN ID.
     * @param vlanIfName VLAN interface name.
     * @param bridgeIfName Bridge interface name.
     */
    NetworkInfo(const String& networkID, const String& subnet, const String& ip, uint64_t vlanID,
        const String& vlanIfName = {}, const String& bridgeIfName = {})
        : mNetworkID(networkID)
        , mSubnet(subnet)
        , mIP(ip)
        , mVlanID(vlanID)
        , mVlanIfName(vlanIfName)
        , mBridgeIfName(bridgeIfName)
    {
    }

    /**
     * Compares network information.
     *
     * @param networkInfo network information to compare.
     * @return bool.
     */
    bool operator==(const NetworkInfo& networkInfo) const
    {
        return mNetworkID == networkInfo.mNetworkID && mSubnet == networkInfo.mSubnet && mIP == networkInfo.mIP
            && mVlanID == networkInfo.mVlanID && mVlanIfName == networkInfo.mVlanIfName
            && mBridgeIfName == networkInfo.mBridgeIfName;
        ;
    }

    /**
     * Compares network information.
     *
     * @param networkInfo network information to compare.
     * @return bool.
     */
    bool operator!=(const NetworkInfo& networkInfo) const { return !operator==(networkInfo); }
};

/**
 * Instance network information.
 */
struct InstanceNetworkInfo {
    StaticString<cIDLen> mInstanceID;
    StaticString<cIDLen> mNetworkID;

    /**
     * Default constructor.
     */
    InstanceNetworkInfo() = default;

    /**
     * Constructor.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     */
    InstanceNetworkInfo(const String& instanceID, const String& networkID)
        : mInstanceID(instanceID)
        , mNetworkID(networkID)
    {
    }
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
     * @param info network information.
     * @return Error.
     */
    virtual Error AddNetworkInfo(const NetworkInfo& info) = 0;

    /**
     * Returns network information.
     *
     * @param networks[out] network information result.
     * @return Error.
     */
    virtual Error GetNetworksInfo(Array<NetworkInfo>& networks) const = 0;

    /**
     * Adds instance network info to storage.
     *
     * @param info instance network information.
     * @return Error.
     */
    virtual Error AddInstanceNetworkInfo(const InstanceNetworkInfo& info) = 0;

    /**
     * Removes instance network info from storage.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error RemoveInstanceNetworkInfo(const String& instanceID) = 0;

    /**
     * Returns instance network info.
     *
     * @param[out] networks instance network information.
     * @return Error.
     */
    virtual Error GetInstanceNetworksInfo(Array<InstanceNetworkInfo>& networks) const = 0;

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

/** @}*/

} // namespace aos::sm::networkmanager

#endif
