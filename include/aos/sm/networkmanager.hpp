/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_NETWORKMANAGER_HPP_
#define AOS_NETWORKMANAGER_HPP_

#include "aos/common/crypto/crypto.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/map.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/common/tools/thread.hpp"
#include "aos/common/types.hpp"
#include "aos/sm/cni.hpp"
#include "aos/sm/config.hpp"

namespace aos::sm::networkmanager {

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
 * Max number of hosts.
 */
static constexpr auto cResolvConfLineLen = AOS_CONFIG_NETWORKMANAGER_RESOLV_CONF_LINE_LEN;

/**
 * Max exposed port len.
 */
static constexpr auto cExposedPortLen = cPortLen + cProtocolNameLen;

/**
 * Max number of hosts.
 */
static constexpr auto cMaxNumHosts = AOS_CONFIG_NETWORKMANAGER_MAX_NUM_HOSTS;

/**
 * Network information.
 */
struct NetworkInfo {
    StaticString<cHostNameLen>  mNetworkID;
    StaticString<cIPLen>        mSubnet;
    StaticString<cIPLen>        mIP;
    uint64_t                    mVlanID;
    StaticString<cInterfaceLen> mVlanIfName;

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
     */
    NetworkInfo(
        const String& networkID, const String& subnet, const String& ip, uint64_t vlanID, const String& vlanIfName = {})
        : mNetworkID(networkID)
        , mSubnet(subnet)
        , mIP(ip)
        , mVlanID(vlanID)
        , mVlanIfName(vlanIfName)
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
            && mVlanID == networkInfo.mVlanID && mVlanIfName == networkInfo.mVlanIfName;
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
 * Traffic period type.
 */
class TrafficPeriodType {
public:
    enum class Enum { eMinutePeriod, eHourPeriod, eDayPeriod, eMonthPeriod, eYearPeriod };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sTrafficPeriodStrings[] = {"minute", "hour", "day", "month", "year"};

        return Array<const char* const>(sTrafficPeriodStrings, ArraySize(sTrafficPeriodStrings));
    };
};

using TrafficPeriodEnum = TrafficPeriodType::Enum;
using TrafficPeriod     = EnumStringer<TrafficPeriodType>;

/**
 * Instance network parameters.
 */
struct InstanceNetworkParameters {
    InstanceIdent                                                   mInstanceIdent;
    aos::NetworkParameters                                          mNetworkParameters;
    StaticString<cHostNameLen>                                      mHostname;
    StaticArray<StaticString<cHostNameLen>, cMaxNumAliases>         mAliases;
    uint64_t                                                        mIngressKbit;
    uint64_t                                                        mEgressKbit;
    StaticArray<StaticString<cExposedPortLen>, cMaxNumExposedPorts> mExposedPorts;
    StaticArray<Host, cMaxNumHosts>                                 mHosts;
    StaticArray<StaticString<cIPLen>, cMaxNumDNSServers>            mDNSSevers;
    StaticString<cFilePathLen>                                      mHostsFilePath;
    StaticString<cFilePathLen>                                      mResolvConfFilePath;
    uint64_t                                                        mUploadLimit;
    uint64_t                                                        mDownloadLimit;

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
            && mDNSSevers == instanceNetworkParams.mDNSSevers && mHostsFilePath == instanceNetworkParams.mHostsFilePath
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
 * Traffic monitor interface.
 */
class TrafficMonitorItf {
public:
    /**
     * Destructor.
     */
    virtual ~TrafficMonitorItf() = default;

    /**
     * Starts traffic monitoring.
     *
     * @return Error.
     */
    virtual Error Start() = 0;

    /**
     * Stops traffic monitoring.
     *
     * @return Error.
     */
    virtual Error Stop() = 0;

    /**
     * Sets monitoring period.
     *
     * @param period monitoring period in seconds.
     */
    virtual void SetPeriod(TrafficPeriod period) = 0;

    /**
     * Starts monitoring instance.
     *
     * @param instanceID instance ID.
     * @param IPAddress instance IP address.
     * @param downloadLimit download limit.
     * @param uploadLimit upload limit.
     * @return Error.
     */
    virtual Error StartInstanceMonitoring(
        const String& instanceID, const String& IPAddress, uint64_t downloadLimit, uint64_t uploadLimit)
        = 0;

    /**
     * Stops monitoring instance.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error StopInstanceMonitoring(const String& instanceID) = 0;

    /**
     * Returns system traffic data.
     *
     * @param inputTraffic input traffic.
     * @param outputTraffic output traffic.
     * @return Error.
     */
    virtual Error GetSystemData(uint64_t& inputTraffic, uint64_t& outputTraffic) const = 0;

    /**
     * Returns instance traffic data.
     *
     * @param instanceID instance ID.
     * @param inputTraffic input traffic.
     * @param outputTraffic output traffic.
     * @return Error.
     */
    virtual Error GetInstanceTraffic(const String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic) const
        = 0;
};

/**
 * Namespace manager interface.
 */
class NamespaceManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NamespaceManagerItf() = default;

    /**
     * Creates network namespace.
     * @param ns network namespace name.
     * @return Error.
     */
    virtual Error CreateNetworkNamespace(const String& ns) = 0;

    /**
     * Returns network namespace path.
     *
     * @param ns network namespace name.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> GetNetworkNamespacePath(const String& ns) const = 0;

    /**
     * Deletes network namespace.
     *
     * @param ns network namespace name.
     * @return Error.
     */
    virtual Error DeleteNetworkNamespace(const String& ns) = 0;
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

/**
 * Network interface manager interface.
 */
class InterfaceManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~InterfaceManagerItf() = default;

    /**
     * Removes interface.
     *
     * @param ifname interface name.
     * @return Error.
     */
    virtual Error DeleteLink(const String& ifname) = 0;

    /**
     * Sets up link.
     *
     * @param ifname interface name.
     * @return Error.
     */
    virtual Error SetupLink(const String& ifname) = 0;

    /**
     * Sets master.
     *
     * @param ifname interface name.
     * @param master master interface name.
     * @return Error.
     */
    virtual Error SetMasterLink(const String& ifname, const String& master) = 0;
};

/**
 * Network interface factory interface.
 */
class InterfaceFactoryItf {
public:
    /**
     * Creates bridge interface.
     *
     * @param name bridge name.
     * @param ip IP address.
     * @param subnet subnet.
     * @return Error.
     */
    virtual Error CreateBridge(const String& name, const String& ip, const String& subnet) = 0;

    /**
     * Creates vlan interface.
     *
     * @param name vlan name.
     * @param vlanID vlan ID.
     * @return Error.
     */
    virtual Error CreateVlan(const String& name, uint64_t vlanID) = 0;

    /**
     * Destructor.
     */
    virtual ~InterfaceFactoryItf() = default;
};

/**
 * Network manager.
 */
class NetworkManager : public NetworkManagerItf {
public:
    /**
     * Creates network manager instance.
     */
    NetworkManager() = default;

    /**
     * Destructor.
     */
    ~NetworkManager();

    /**
     * Initializes network manager.
     *
     * @param storage storage interface.
     * @param cni CNI interface.
     * @param netMonitor traffic monitor.
     * @param netns namespace manager.
     * @param netIf network interface manager.
     * @param workingDir working directory.
     * @return Error.
     */
    Error Init(StorageItf& storage, cni::CNIItf& cni, TrafficMonitorItf& netMonitor, NamespaceManagerItf& netns,
        InterfaceManagerItf& netIf, crypto::RandomItf& random, InterfaceFactoryItf& netIfFactory,
        const String& workingDir);

    /**
     * Starts network manager.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops network manager.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Returns instance's network namespace path.
     *
     * @param instanceID instance ID.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    RetWithError<StaticString<cFilePathLen>> GetNetnsPath(const String& instanceID) const override;

    /**
     * Updates networks.
     *
     * @param networks network parameters.
     * @return Error.
     */
    Error UpdateNetworks(const Array<aos::NetworkParameters>& networks) override;

    /**
     * Adds instance to network.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @param instanceNetworkParameters instance network parameters.
     * @return Error.
     */
    Error AddInstanceToNetwork(const String& instanceID, const String& networkID,
        const InstanceNetworkParameters& instanceNetworkParameters) override;

    /**
     * Removes instance from network.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @return Error.
     */
    Error RemoveInstanceFromNetwork(const String& instanceID, const String& networkID) override;

    /**
     * Returns instance's IP address.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @param[out] ip instance's IP address.
     * @return Error.
     */
    Error GetInstanceIP(const String& instanceID, const String& networkID, String& ip) const override;

    /**
     * Returns instance's traffic.
     *
     * @param instanceID instance ID.
     * @param[out] inputTraffic instance's input traffic.
     * @param[out] outputTraffic instance's output traffic.
     * @return Error.
     */
    Error GetInstanceTraffic(const String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic) const override;

    /**
     * Gets system traffic.
     *
     * @param[out] inputTraffic system input traffic.
     * @param[out] outputTraffic system output traffic.
     * @return Error.
     */
    Error GetSystemTraffic(uint64_t& inputTraffic, uint64_t& outputTraffic) const override;

    /**
     * Sets the traffic period.
     *
     * @param period traffic period.
     * @return Error
     */
    Error SetTrafficPeriod(TrafficPeriod period) override;

private:
    struct NetworkData {
        StaticString<cIPLen>                                  mIPAddr;
        StaticArray<StaticString<cHostNameLen>, cMaxNumHosts> mHost;
    };

    using InstanceCache = StaticMap<StaticString<cInstanceIDLen>, NetworkData, cMaxNumInstances>;
    using NetworkCache  = StaticMap<StaticString<cProviderIDLen>, InstanceCache, cMaxNumServiceProviders>;

    static constexpr uint64_t cBurstLen                  = 12800;
    static constexpr auto     cMaxExposedPort            = 2;
    static constexpr auto     cCountRetriesVlanIfNameGen = 10;
    static constexpr auto     cAdminChainPrefix          = "INSTANCE_";
    static constexpr auto     cInstanceInterfaceName     = "eth0";
    static constexpr auto     cBridgePrefix              = "br-";
    static constexpr auto     cVlanIfPrefix              = "vlan-";
    static constexpr auto     cNumAllocations            = 8 * AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES;

    Error IsInstanceInNetwork(const String& instanceID, const String& networkID) const;
    Error AddInstanceToCache(const String& instanceID, const String& networkID);
    Error PrepareCNIConfig(const String& instanceID, const String& networkID, const InstanceNetworkParameters& network,
        cni::NetworkConfigList& net, cni::RuntimeConf& rt, Array<StaticString<cHostNameLen>>& hosts) const;
    Error PrepareNetworkConfigList(const String& instanceID, const String& networkID,
        const InstanceNetworkParameters& network, cni::NetworkConfigList& net) const;
    Error PrepareRuntimeConfig(
        const String& instanceID, cni::RuntimeConf& rt, const Array<StaticString<cHostNameLen>>& hosts) const;

    Error CreateBridgePluginConfig(
        const String& networkID, const InstanceNetworkParameters& network, cni::BridgePluginConf& config) const;
    Error CreateFirewallPluginConfig(
        const String& instanceID, const InstanceNetworkParameters& network, cni::FirewallPluginConf& config) const;
    Error CreateBandwidthPluginConfig(const InstanceNetworkParameters& network, cni::BandwidthNetConf& config) const;
    Error CreateDNSPluginConfig(
        const String& networkID, const InstanceNetworkParameters& network, cni::DNSPluginConf& config) const;
    Error UpdateInstanceNetworkCache(const String& instanceID, const String& networkID, const String& instanceIP,
        const Array<StaticString<cHostNameLen>>& hosts);
    Error RemoveInstanceFromCache(const String& instanceID, const String& networkID);
    Error ClearNetwork(const String& networkID);
    Error PrepareHosts(const String& instanceID, const String& networkID, const InstanceNetworkParameters& network,
        Array<StaticString<cHostNameLen>>& hosts) const;
    Error IsHostnameExist(const InstanceCache& instanceCache, const Array<StaticString<cHostNameLen>>& hosts) const;
    Error PushHostWithDomain(
        const String& host, const String& networkID, Array<StaticString<cHostNameLen>>& hosts) const;
    Error CreateHostsFile(
        const String& networkID, const String& instanceIP, const InstanceNetworkParameters& network) const;
    Error WriteHost(const Host& host, int fd) const;
    Error WriteHosts(Array<SharedPtr<Host>> hosts, int fd) const;
    Error WriteHosts(Array<Host> hosts, int fd) const;
    Error WriteHostsFile(
        const String& filePath, const Array<SharedPtr<Host>>& hosts, const InstanceNetworkParameters& network) const;

    Error CreateResolvConfFile(const String& networkID, const InstanceNetworkParameters& network,
        const Array<StaticString<cIPLen>>& dns) const;
    Error WriteResolvConfFile(const String& filePath, const Array<StaticString<cIPLen>>& mainServers,
        const InstanceNetworkParameters& network) const;

    Error RemoveNetworks(const Array<aos::NetworkParameters>& networks);
    Error RemoveNetwork(const String& networkID);
    Error CreateNetwork(const NetworkInfo& network);
    Error GenerateVlanIfName(String& vlanIfName);

    StorageItf*                                                                   mStorage {};
    cni::CNIItf*                                                                  mCNI {};
    TrafficMonitorItf*                                                            mNetMonitor {};
    NamespaceManagerItf*                                                          mNetns {};
    InterfaceManagerItf*                                                          mNetIf {};
    crypto::RandomItf*                                                            mRandom {};
    InterfaceFactoryItf*                                                          mNetIfFactory {};
    StaticString<cFilePathLen>                                                    mCNINetworkCacheDir;
    NetworkCache                                                                  mNetworkData;
    StaticMap<StaticString<cProviderIDLen>, NetworkInfo, cMaxNumServiceProviders> mNetworkProviders;
    StaticAllocator<sizeof(NetworkInfo)>                                          mNetworkInfoAllocator;
    StaticAllocator<sizeof(StaticArray<NetworkInfo, cMaxNumServiceProviders>)>    mNetworkInfosAllocator;
    mutable Mutex                                                                 mMutex;
    StaticAllocator<(sizeof(cni::NetworkConfigList) + sizeof(cni::RuntimeConf) + sizeof(cni::Result))
            * AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES,
        cNumAllocations>
        mAllocator;
    mutable StaticAllocator<(sizeof(Host) * 3) * AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES, cNumAllocations>
        mHostAllocator;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
