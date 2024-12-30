/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SM_CNI_HPP_
#define AOS_SM_CNI_HPP_

#include "aos/common/tools/string.hpp"
#include "aos/common/tools/uuid.hpp"
#include "aos/common/types.hpp"
#include "aos/sm/config.hpp"

namespace aos::sm::cni {

/**
 * Max CNI version length.
 */
constexpr auto cVersionLen = AOS_CONFIG_CNI_VERSION_LEN;

/**
 * Max CNI plugin name length.
 */
constexpr auto cNameLen = AOS_CONFIG_CNI_PLUGIN_NAME_LEN;

/**
 * Max CNI plugin type length.
 */
constexpr auto cPluginTypeLen = AOS_CONFIG_CNI_PLUGIN_TYPE_LEN;

/**
 * Max CNI router number.
 */
constexpr auto cMaxNumRouters = AOS_CONFIG_CNI_MAX_NUM_ROUTERS;

/**
 * Max CNI number of interfaces.
 */
constexpr auto cMaxNumInterfaces = AOS_CONFIG_CNI_MAX_NUM_INTERFACES;

/**
 * Max CNI number of IPs.
 */
constexpr auto cMaxNumIPs = AOS_CONFIG_CNI_MAX_NUM_IPS;

/**
 * Max CNI number of runtime config arguments.
 */
constexpr auto cMaxNumRuntimeConfigArgs = AOS_CONFIG_CNI_MAX_NUM_RUNTIME_CONFIG_ARGS;

/**
 * Max CNI runtime config argument length.
 */
constexpr auto cRuntimeConfigArgLen = AOS_CONFIG_CNI_RUNTIME_CONFIG_ARG_LEN;

/**
 * CNI version.
 */
constexpr auto cVersion = "0.4.0";

/**
 * Network router.
 */
struct Router {
    StaticString<cSubnetLen> mDst;
    StaticString<cIPLen>     mGW;
};

/**
 * Range IP.
 */
struct Range {
    StaticString<cSubnetLen> mSubnet;
    StaticString<cIPLen>     mRangeStart;
    StaticString<cIPLen>     mRangeEnd;
};

/**
 * IPAM configuration.
 */
struct IPAM {
    StaticString<cNameLen>              mName;
    StaticString<cPluginTypeLen>        mType;
    Range                               mRange;
    StaticString<cFilePathLen>          mDataDir;
    StaticArray<Router, cMaxNumRouters> mRouters;
};

/**
 * Bridge plugin configuration.
 */
struct BridgePluginConf {
    StaticString<cPluginTypeLen> mType;
    StaticString<cInterfaceLen>  mBridge;
    bool                         mIsGateway;
    bool                         mIPMasq;
    bool                         mHairpinMode;
    IPAM                         mIPAM;
};

/**
 * IPs information.
 */
struct IPs {
    StaticString<cVersionLen> mVersion;
    int                       mInterface;
    StaticString<cSubnetLen>  mAddress;
    StaticString<cIPLen>      mGateway;
};

/**
 * Interface information.
 */
struct Interface {
    StaticString<cInterfaceLen> mName;
    StaticString<cMacLen>       mMac;
    StaticString<cFilePathLen>  mSandbox;
};

/**
 * Result of a CNI operation.
 *
 */
struct Result {
    StaticString<cVersionLen>                            mVersion;
    StaticString<cNameLen>                               mName;
    StaticArray<Interface, cMaxNumRouters>               mInterfaces;
    StaticArray<IPs, cMaxNumRouters>                     mIPs;
    StaticArray<Router, cMaxNumRouters>                  mRoutes;
    StaticArray<StaticString<cIPLen>, cMaxNumDNSServers> mDNSServers;
};

/**
 * Input access configuration.
 */
struct InputAccessConfig {
    StaticString<cPortLen>         mPort;
    StaticString<cProtocolNameLen> mProtocol;
};

/**
 * Output access configuration.
 */
struct OutputAccessConfig {
    StaticString<cSubnetLen>       mDstIP;
    StaticString<cPortLen>         mDstPort;
    StaticString<cProtocolNameLen> mProto;
    StaticString<cSubnetLen>       mSrcIP;
};

/**
 * Firewall plugin configuration.
 */
struct FirewallPluginConf {
    StaticString<cPluginTypeLen>                          mType;
    StaticString<uuid::cUUIDLen>                          mUUID;
    StaticString<cIptablesChainNameLen>                   mIptablesAdminChainName;
    bool                                                  mAllowPublicConnections;
    StaticArray<InputAccessConfig, cMaxNumFirewallRules>  mInputAccess;
    StaticArray<OutputAccessConfig, cMaxNumFirewallRules> mOutputAccess;
};

/**
 * Capabilities.
 */
struct Capabilities {
    bool mAliases;
};

/**
 * DNS plugin configuration.
 */
struct DNSPluginConf {
    StaticString<cPluginTypeLen>                         mType;
    bool                                                 mMultiDomain;
    StaticString<cHostNameLen>                           mDomainName;
    Capabilities                                         mCapabilities;
    StaticArray<StaticString<cIPLen>, cMaxNumDNSServers> mRemoteServers;
};

/**
 * Capability arguments.
 */
struct CapabilityArgs {
    StaticArray<StaticString<cHostNameLen>, cMaxNumDNSServers * cMaxNumInstances> mHost;
};

/**
 * Argument.
 */
struct Arg {
    StaticString<cRuntimeConfigArgLen> mName;
    StaticString<cRuntimeConfigArgLen> mValue;
};

/**
 * Runtime configuration parameters.
 *
 */
struct RuntimeConf {
    StaticString<cInstanceIDLen>     mContainerID;
    StaticString<cFilePathLen>       mNetNS;
    StaticString<cInterfaceLen>      mIfName;
    StaticArray<Arg, cMaxNumRouters> mArgs;
    StaticString<cFilePathLen>       mCacheDir;
    CapabilityArgs                   mCapabilityArgs;
};

/**
 * Bandwidth network configuration.
 *
 */
struct BandwidthNetConf {
    StaticString<cPluginTypeLen> mType;
    uint64_t                     mIngressRate;
    uint64_t                     mIngressBurst;
    uint64_t                     mEgressRate;
    uint64_t                     mEgressBurst;
};

/**
 * Network configuration.
 *
 */
struct NetworkConfigList {
    StaticString<cVersionLen> mVersion;
    StaticString<cNameLen>    mName;
    BridgePluginConf          mBridge;
    FirewallPluginConf        mFirewall;
    BandwidthNetConf          mBandwidth;
    DNSPluginConf             mDNS;
    Result                    mPrevResult;
};

/**
 * CNI interface.
 *
 */
class CNIItf {
public:
    /**
     * Destructor.
     */
    virtual ~CNIItf() = default;

    /**
     * Initializes CNI.
     *
     * @param cniConfigDir Path to CNI configuration directory.
     * @return Error.
     */
    virtual Error Init(const String& configDir) = 0;

    /**
     * Executes a sequence of plugins with the ADD command
     *
     * @param net List of network configurations.
     * @param rt Runtime configuration parameters.
     * @return RetWithError<Result>.
     */
    virtual RetWithError<Result> AddNetworkList(const NetworkConfigList& net, const RuntimeConf& rt) = 0;

    /**
     * Executes a sequence of plugins with the DEL command
     *
     * @param net List of network configurations.
     * @param rt Runtime configuration parameters.
     * @return Error.
     */
    virtual Error DeleteNetworkList(const NetworkConfigList& net, const RuntimeConf& rt) = 0;

    /**
     * Checks that a configuration is reasonably valid.
     *
     * @param net List of network configurations.
     * @return Error.
     */
    virtual Error ValidateNetworkList(const NetworkConfigList& net) = 0;
};

} // namespace aos::sm::cni

#endif // AOS_SM_CNI_HPP_
