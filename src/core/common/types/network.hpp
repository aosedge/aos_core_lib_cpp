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
    bool operator==(const FirewallRule& rule) const
    {
        return mDstIP == rule.mDstIP && mDstPort == rule.mDstPort && mProto == rule.mProto && mSrcIP == rule.mSrcIP;
    }

    /**
     * Compares firewall rule.
     *
     * @param rule firewall rule to compare.
     * @return bool.
     */
    bool operator!=(const FirewallRule& rule) const { return !operator==(rule); }
};

/**
 * Networks parameters.
 */
struct NetworkParameters {
    StaticString<cHostNameLen>                                 mNetworkID;
    StaticString<cSubnetLen>                                   mSubnet;
    StaticString<cIPLen>                                       mIP;
    uint64_t                                                   mVlanID = 0;
    StaticArray<StaticString<cHostNameLen>, cMaxNumDNSServers> mDNSServers;
    StaticArray<FirewallRule, cMaxNumFirewallRules>            mFirewallRules;

    /**
     * Compares network parameters.
     *
     * @param networkParams network parameters to compare.
     * @return bool.
     */
    bool operator==(const NetworkParameters& networkParams) const
    {
        return mNetworkID == networkParams.mNetworkID && mSubnet == networkParams.mSubnet && mIP == networkParams.mIP
            && mVlanID == networkParams.mVlanID && mDNSServers == networkParams.mDNSServers
            && mFirewallRules == networkParams.mFirewallRules;
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
    bool operator==(const Host& rhs) const { return mIP == rhs.mIP && mHostname == rhs.mHostname; }

    /**
     * Compares host.
     *
     * @param rhs host to compare.
     * @return bool.
     */
    bool operator!=(const Host& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif // AOS_CORE_COMMON_TYPES_NETWORK_HPP_
