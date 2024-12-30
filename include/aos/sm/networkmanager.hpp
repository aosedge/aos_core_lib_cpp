/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_NETWORKMANAGER_HPP_
#define AOS_NETWORKMANAGER_HPP_

#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/map.hpp"
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
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error CreateNetworkNamespace(const String& instanceID) = 0;

    /**
     * Returns network namespace path.
     *
     * @param instanceID instance ID.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> GetNetworkNamespacePath(const String& instanceID) const = 0;

    /**
     * Deletes network namespace.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error DeleteNetworkNamespace(const String& instanceID) = 0;
};

/**
 * Network interface manager interface.
 */
class NetworkInterfaceManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NetworkInterfaceManagerItf() = default;

    /**
     * Removes interface.
     *
     * @param ifname interface name.
     * @return Error.
     */
    virtual Error RemoveInterface(const String& ifname) = 0;
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
        NetworkInterfaceManagerItf& netIf, const String& workingDir);

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
     * @param network network parameters.
     * @return Error.
     */
    Error AddInstanceToNetwork(
        const String& instanceID, const String& networkID, const NetworkParams& network) override;

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
        StaticString<cIPLen>                                  IPAddr;
        StaticArray<StaticString<cHostNameLen>, cMaxNumHosts> mHost;
    };

    using InstanceCache = StaticMap<StaticString<cInstanceIDLen>, NetworkData, cMaxNumInstances>;
    using NetworkCache  = StaticMap<StaticString<cProviderIDLen>, InstanceCache, cMaxNumServiceProviders>;

    static constexpr uint64_t cBurstLen              = 12800;
    static constexpr auto     cMaxExposedPort        = 2;
    static constexpr auto     cAdminChainPrefix      = "INSTANCE_";
    static constexpr auto     cInstanceInterfaceName = "eth0";
    static constexpr auto     cBridgePrefix          = "br-";

    Error IsInstanceInNetwork(const String& instanceID, const String& networkID) const;
    Error AddInstanceToCache(const String& instanceID, const String& networkID);
    Error PrepareCNIConfig(const String& instanceID, const String& networkID, const NetworkParams& network,
        cni::NetworkConfigList& net, cni::RuntimeConf& rt, Array<StaticString<cHostNameLen>>& hosts) const;
    Error PrepareNetworkConfigList(const String& instanceID, const String& networkID, const NetworkParams& network,
        cni::NetworkConfigList& net) const;
    Error PrepareRuntimeConfig(
        const String& instanceID, cni::RuntimeConf& rt, const Array<StaticString<cHostNameLen>>& hosts) const;

    Error CreateBridgePluginConfig(
        const String& networkID, const NetworkParams& network, cni::BridgePluginConf& config) const;
    Error CreateFirewallPluginConfig(
        const String& instanceID, const NetworkParams& network, cni::FirewallPluginConf& config) const;
    Error CreateBandwidthPluginConfig(const NetworkParams& network, cni::BandwidthNetConf& config) const;
    Error CreateDNSPluginConfig(
        const String& networkID, const NetworkParams& network, cni::DNSPluginConf& config) const;
    Error UpdateInstanceNetworkCache(const String& instanceID, const String& networkID, const String& instanceIP,
        const Array<StaticString<cHostNameLen>>& hosts);
    Error RemoveInstanceFromCache(const String& instanceID, const String& networkID);
    Error ClearNetwork(const String& networkID);
    Error PrepareHosts(const String& instanceID, const String& networkID, const NetworkParams& network,
        Array<StaticString<cHostNameLen>>& hosts) const;
    Error IsHostnameExist(const InstanceCache& instanceCache, const Array<StaticString<cHostNameLen>>& hosts) const;
    Error PushHostWithDomain(
        const String& host, const String& networkID, Array<StaticString<cHostNameLen>>& hosts) const;
    Error CreateHostsFile(const String& networkID, const String& instanceIP, const NetworkParams& network) const;
    Error WriteHostsFile(const String& filePath, const Array<Host>& hosts, const NetworkParams& network) const;

    Error CreateResolvConfFile(
        const String& networkID, const NetworkParams& network, const Array<StaticString<cIPLen>>& dns) const;
    Error WriteResolvConfFile(
        const String& filePath, const Array<StaticString<cIPLen>>& mainServers, const NetworkParams& network) const;

    StorageItf*                 mStorage {};
    cni::CNIItf*                mCNI {};
    TrafficMonitorItf*          mNetMonitor {};
    NamespaceManagerItf*        mNetns {};
    NetworkInterfaceManagerItf* mNetIf {};
    StaticString<cFilePathLen>  mCNINetworkCacheDir;
    NetworkCache                mNetworkData;
    mutable Mutex               mMutex;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
