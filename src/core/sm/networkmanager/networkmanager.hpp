/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_NETWORKMANAGER_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_NETWORKMANAGER_HPP_

#include <core/common/crypto/itf/hash.hpp>
#include <core/common/crypto/itf/rand.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/thread.hpp>

#include "itf/cni.hpp"
#include "itf/networkmanager.hpp"

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

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
     * @param random random generator interface.
     * @param netIfFactory network interface factory.
     * @param hasher hasher interface.
     * @param workingDir working directory.
     * @return Error.
     */
    Error Init(StorageItf& storage, cni::CNIItf& cni, TrafficMonitorItf& netMonitor, NamespaceManagerItf& netns,
        InterfaceManagerItf& netIf, crypto::RandomItf& random, InterfaceFactoryItf& netIfFactory,
        crypto::HasherItf& hasher, const String& workingDir);

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

    using InstanceCache = StaticMap<StaticString<cIDLen>, NetworkData, cMaxNumInstances>;
    using NetworkCache  = StaticMap<StaticString<cIDLen>, InstanceCache, cMaxNumOwners>;

    static constexpr uint64_t cBurstLen                  = 12800;
    static constexpr auto     cMaxExposedPort            = 2;
    static constexpr auto     cCountRetriesVlanIfNameGen = 10;
    static constexpr auto     cAdminChainPrefix          = "INSTANCE_";
    static constexpr auto     cInstanceInterfaceName     = "eth0";
    static constexpr auto     cBridgePrefix              = "br-";
    static constexpr auto     cVlanIfPrefix              = "vlan-";
    static constexpr auto     cMaxBridgeNetworkIDLen     = cInterfaceLen - 1 - strlen(cBridgePrefix); // 15 - 3 = 12
    static constexpr auto     cNumAllocations            = 8 * cMaxNumConcurrentItems;
    static constexpr auto     cResolvConfLineLen         = AOS_CONFIG_NETWORKMANAGER_RESOLV_CONF_LINE_LEN;

    Error IsInstanceInNetwork(const String& instanceID, const String& networkID) const;
    Error AddInstanceToCache(const String& instanceID, const String& networkID);
    Error PrepareCNIConfig(const String& instanceID, const String& networkID, const InstanceNetworkParameters& network,
        cni::NetworkConfigList& net, cni::RuntimeConf& rt, Array<StaticString<cHostNameLen>>& hosts);
    Error PrepareNetworkConfigList(const String& instanceID, const String& networkID,
        const InstanceNetworkParameters& network, cni::NetworkConfigList& net);
    Error PrepareRuntimeConfig(
        const String& instanceID, cni::RuntimeConf& rt, const Array<StaticString<cHostNameLen>>& hosts) const;

    Error CreateBridgePluginConfig(const String& networkID, const String& bridgeName,
        const InstanceNetworkParameters& network, cni::BridgePluginConf& config) const;
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
    Error WriteHosts(const Array<SharedPtr<Host>>& hosts, int fd) const;
    Error WriteHosts(const Array<Host>& hosts, int fd) const;
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
    Error GenerateBridgeName(const String& networkID, String& bridgeName);
    Error DeleteInstanceNetworkConfig(const String& instanceID, const String& networkID);
    Error CleanupInstanceNetworkResources(const String& instanceID, const String& networkID);

    StorageItf*                                                                 mStorage {};
    cni::CNIItf*                                                                mCNI {};
    TrafficMonitorItf*                                                          mNetMonitor {};
    NamespaceManagerItf*                                                        mNetns {};
    InterfaceManagerItf*                                                        mNetIf {};
    crypto::RandomItf*                                                          mRandom {};
    InterfaceFactoryItf*                                                        mNetIfFactory {};
    crypto::HasherItf*                                                          mHasher {};
    StaticString<cFilePathLen>                                                  mCNINetworkCacheDir;
    NetworkCache                                                                mNetworkData;
    StaticMap<StaticString<cIDLen>, NetworkInfo, cMaxNumOwners>                 mNetworkProviders;
    StaticAllocator<sizeof(NetworkInfo)>                                        mNetworkInfoAllocator;
    StaticAllocator<sizeof(StaticArray<NetworkInfo, cMaxNumOwners>)>            mNetworkInfosAllocator;
    StaticAllocator<sizeof(StaticArray<InstanceNetworkInfo, cMaxNumInstances>)> mInstanceNetworkInfosAllocator;

    mutable Mutex mMutex;
    StaticAllocator<(sizeof(cni::NetworkConfigList) + sizeof(cni::RuntimeConf) + sizeof(cni::Result))
            * cMaxNumConcurrentItems,
        cNumAllocations>
                                                                                          mAllocator;
    mutable StaticAllocator<(sizeof(Host) * 3) * cMaxNumConcurrentItems, cNumAllocations> mHostAllocator;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
