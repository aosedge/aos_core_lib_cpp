/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_NETWORK_HPP_
#define AOS_CORE_COMMON_TYPES_NETWORK_HPP_

#include "common.hpp"

namespace aos {

/**
 * IP len.
 */
constexpr auto cIPLen = AOS_CONFIG_TYPES_IP_LEN;

/**
 * Port len.
 */
constexpr auto cPortLen = AOS_CONFIG_TYPES_PORT_LEN;

/**
 * Protocol name len.
 */
constexpr auto cProtocolNameLen = AOS_CONFIG_TYPES_PROTOCOL_NAME_LEN;

/**
 * Max number of DNS servers.
 */
constexpr auto cMaxNumDNSServers = AOS_CONFIG_TYPES_MAX_NUM_DNS_SERVERS;

/**
 * Max number of firewall rules.
 */
constexpr auto cMaxNumFirewallRules = AOS_CONFIG_TYPES_MAX_NUM_FIREWALL_RULES;

/**
 * Host name len.
 */
constexpr auto cHostNameLen = AOS_CONFIG_TYPES_HOST_NAME_LEN;

/**
 * Max subnet len.
 */
static constexpr auto cSubnetLen = AOS_CONFIG_TYPES_SUBNET_LEN;

/**
 * Max MAC len.
 */
static constexpr auto cMACLen = AOS_CONFIG_TYPES_MAC_LEN;

/**
 * Max iptables chain name length.
 */
static constexpr auto cIptablesChainNameLen = AOS_CONFIG_TYPES_IPTABLES_CHAIN_LEN;

/**
 * Max CNI interface name length.
 */
static constexpr auto cInterfaceLen = AOS_CONFIG_TYPES_INTERFACE_NAME_LEN;

/**
 * Max number of exposed ports.
 */
static constexpr auto cMaxNumExposedPorts = AOS_CONFIG_TYPES_MAX_NUM_EXPOSED_PORTS;

/**
 * Max exposed port len.
 */
static constexpr auto cExposedPortLen = cPortLen + cProtocolNameLen;

/**
 * Max length of connection name.
 */
static constexpr auto cConnectionNameLen = cIDLen + cExposedPortLen;

/**
 * Max number of allowed connections.
 */
static constexpr auto cMaxNumConnections = AOS_CONFIG_TYPES_MAX_NUM_ALLOWED_CONNECTIONS;

/**
 * Max number of hosts.
 */
constexpr auto cMaxNumHosts = AOS_CONFIG_TYPES_MAX_NUM_HOSTS;

/**
 * Firewall rule.
 */
struct FirewallRule {
    StaticString<cIPLen>           mDstIP;
    StaticString<cPortLen>         mDstPort;
    StaticString<cProtocolNameLen> mProto;
    StaticString<cIPLen>           mSrcIP;

    /**
     * Compares firewall rule.
     *
     * @param rule firewall rule to compare.
     * @return bool.
     */
    friend bool operator==(const FirewallRule& lhs, const FirewallRule& rule)
    {
        return lhs.mDstIP == rule.mDstIP && lhs.mDstPort == rule.mDstPort && lhs.mProto == rule.mProto
            && lhs.mSrcIP == rule.mSrcIP;
    };

    /**
     * Compares firewall rule.
     *
     * @param rule firewall rule to compare.
     * @return bool.
     */
    friend bool operator!=(const FirewallRule& lhs, const FirewallRule& rule) { return !(lhs == rule); };
};

/**
 * Networks parameters.
 */
struct NetworkParams {
    StaticString<cHostNameLen> mNetworkID;
    StaticString<cSubnetLen>   mSubnet;
    StaticString<cIPLen>       mIP;
    uint64_t                   mVlanID {};

    /**
     * Compares network parameters.
     *
     * @param rhs network parameters to compare.
     * @return bool.
     */
    friend bool operator==(const NetworkParams& lhs, const NetworkParams& rhs)
    {
        return lhs.mNetworkID == rhs.mNetworkID && lhs.mSubnet == rhs.mSubnet && lhs.mIP == rhs.mIP
            && lhs.mVlanID == rhs.mVlanID;
    };

    /**
     * Compares network parameters.
     *
     * @param rhs network parameters to compare.
     * @return bool.
     */
    friend bool operator!=(const NetworkParams& lhs, const NetworkParams& rhs) { return !(lhs == rhs); };
};

/**
 * Instance network parameters.
 */
struct InstanceNetworkAllocation {
    StaticString<cHostNameLen>                           mNetworkID;
    StaticString<cSubnetLen>                             mSubnet;
    StaticString<cIPLen>                                 mIP;
    StaticArray<StaticString<cIPLen>, cMaxNumDNSServers> mDNSServers;
    StaticArray<FirewallRule, cMaxNumFirewallRules>      mFirewallRules;

    /**
     * Compares instance network parameters.
     *
     * @param rhs instance network parameters to compare.
     * @return bool.
     */
    friend bool operator==(const InstanceNetworkAllocation& lhs, const InstanceNetworkAllocation& rhs)
    {
        return lhs.mNetworkID == rhs.mNetworkID && lhs.mSubnet == rhs.mSubnet && lhs.mIP == rhs.mIP
            && lhs.mDNSServers == rhs.mDNSServers && lhs.mFirewallRules == rhs.mFirewallRules;
    };

    /**
     * Compares instance network parameters.
     *
     * @param rhs instance network parameters to compare.
     * @return bool.
     */
    friend bool operator!=(const InstanceNetworkAllocation& lhs, const InstanceNetworkAllocation& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * Item network data.
 */
struct UpdateItemNetworkParams {
    StaticArray<StaticString<cHostNameLen>, cMaxNumHosts>             mHosts;
    StaticArray<StaticString<cConnectionNameLen>, cMaxNumConnections> mAllowedConnections;
    StaticArray<StaticString<cExposedPortLen>, cMaxNumExposedPorts>   mExposedPorts;

    /**
     * Compares item network data.
     *
     * @param rhs item network data to compare.
     * @return bool.
     */
    friend bool operator==(const UpdateItemNetworkParams& lhs, const UpdateItemNetworkParams& rhs)
    {
        return lhs.mHosts == rhs.mHosts && lhs.mAllowedConnections == rhs.mAllowedConnections
            && lhs.mExposedPorts == rhs.mExposedPorts;
    };

    /**
     * Compares item network data.
     *
     * @param rhs item network data to compare.
     * @return bool.
     */
    friend bool operator!=(const UpdateItemNetworkParams& lhs, const UpdateItemNetworkParams& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * Host.
 */
struct Host {
    StaticString<cHostNameLen> mHostname;
    StaticString<cIPLen>       mIP;

    /**
     * Default constructor.
     */
    Host() = default;

    /**
     * Constructs host.
     *
     * @param ip IP.
     * @param hostname hostname.
     */
    Host(const String& ip, const String& hostname)
        : mHostname(hostname)
        , mIP(ip)

    {
    }

    /**
     * Compares host.
     *
     * @param rhs host to compare.
     * @return bool.
     */
    friend bool operator==(const Host& lhs, const Host& rhs)
    {
        return lhs.mIP == rhs.mIP && lhs.mHostname == rhs.mHostname;
    };

    /**
     * Compares host.
     *
     * @param rhs host to compare.
     * @return bool.
     */
    friend bool operator!=(const Host& lhs, const Host& rhs) { return !(lhs == rhs); };
};

/**
 * Instance network state info for sync on (re)connect.
 */
struct InstanceNetworkStateInfo {
    InstanceIdent                                   mInstanceIdent;
    StaticString<cIDLen>                            mNetworkID;
    StaticString<cIPLen>                            mIP;
    StaticArray<FirewallRule, cMaxNumFirewallRules> mFirewallRules;

    /**
     * Default constructor.
     */
    InstanceNetworkStateInfo() = default;

    /**
     * Constructor.
     *
     * @param instanceIdent instance identifier.
     * @param networkID network identifier.
     * @param ip IP address.
     * @param firewallRules firewall rules.
     */
    InstanceNetworkStateInfo(const InstanceIdent& instanceIdent, const String& networkID, const String& ip,
        const Array<FirewallRule>& firewallRules)
        : mInstanceIdent(instanceIdent)
        , mNetworkID(networkID)
        , mIP(ip)
        , mFirewallRules(firewallRules)
    {
    }

    /**
     * Compares instance network state info.
     *
     * @param rhs instance network state info to compare.
     * @return bool.
     */
    friend bool operator==(const InstanceNetworkStateInfo& lhs, const InstanceNetworkStateInfo& rhs)
    {
        return lhs.mInstanceIdent == rhs.mInstanceIdent && lhs.mNetworkID == rhs.mNetworkID && lhs.mIP == rhs.mIP
            && lhs.mFirewallRules == rhs.mFirewallRules;
    };

    /**
     * Compares instance network state info.
     *
     * @param rhs instance network state info to compare.
     * @return bool.
     */
    friend bool operator!=(const InstanceNetworkStateInfo& lhs, const InstanceNetworkStateInfo& rhs)
    {
        return !(lhs == rhs);
    };
};

} // namespace aos

#endif
