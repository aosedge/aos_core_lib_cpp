/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_NETWORKMANAGER_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_NETWORKMANAGER_HPP_

#include <core/common/crypto/itf/rand.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/thread.hpp>

#include <core/common/networkmanager/itf/networkprovider.hpp>
#include <core/common/networkmanager/itf/pendingupdatehandler.hpp>

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
     * @param workingDir working directory.
     * @return Error.
     */
    Error Init(StorageItf& storage, cni::CNIItf& cni, TrafficMonitorItf& netMonitor, NamespaceManagerItf& netns,
        InterfaceManagerItf& netIf, crypto::RandomItf& random, InterfaceFactoryItf& netIfFactory,
        const String& workingDir, aos::networkmanager::NetworkProviderItf& networkProvider, const String& nodeID);

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

    /**
     * Creates instance network.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @param networkConfig instance network config.
     * @return Error.
     */
    Error CreateInstanceNetwork(
        const String& instanceID, const String& networkID, const InstanceNetworkConfig& networkConfig) override;

    /**
     * Starts instance network.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @param runtimeParams runtime parameters.
     * @return Error.
     */
    Error StartInstanceNetwork(
        const String& instanceID, const String& networkID, const InstanceNetworkRuntimeParams& runtimeParams) override;

    /**
     * Stops instance network.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @return Error.
     */
    Error StopInstanceNetwork(const String& instanceID, const String& networkID) override;

    /**
     * Releases instance network.
     *
     * @param instanceID instance ID.
     * @param networkID network ID.
     * @return Error.
     */
    Error ReleaseInstanceNetwork(const String& instanceID, const String& networkID) override;

    /**
     * Called when pending firewall rules are resolved for an instance.
     *
     * @param nodeID node ID where the instance resides.
     * @param update pending firewall update.
     */
    void OnPendingFirewallUpdate(
        const String& nodeID, const aos::networkmanager::PendingFirewallUpdate& update) override;

private:
    Error EnsureNodeNetwork(const String& networkID);
    Error EnsureNodeNetworkPhysical(const String& networkID);
    Error UpdateInstanceFirewall(const String& instanceID, const String& networkID,
        const InstanceNetworkConfig& networkConfig, const aos::InstanceNetworkAllocation& networkParams);

    Error AddInstanceToNetwork(const String& instanceID, const String& networkID,
        const InstanceNetworkConfig& networkConfig, const aos::InstanceNetworkAllocation& networkParams,
        const InstanceNetworkRuntimeParams& runtimeParams);

    using InstanceHosts = StaticArray<StaticString<cHostNameLen>, cMaxNumHosts>;
    using InstanceCache = StaticMap<StaticString<cIDLen>, InstanceHosts, cMaxNumInstances>;
    using NetworkCache  = StaticMap<StaticString<cIDLen>, InstanceCache, cMaxNumOwners>;

    static constexpr uint64_t cBurstLen              = 12800;
    static constexpr auto     cMaxExposedPort        = 2;
    static constexpr auto     cCountRetriesIfNameGen = 10;
    static constexpr auto     cMaxNetworkIDLen       = 8;
    static constexpr auto     cAdminChainPrefix      = "INSTANCE_";
    static constexpr auto     cInstanceInterfaceName = "eth0";
    static constexpr auto     cBridgePrefix          = "br-";
    static constexpr auto     cVlanIfPrefix          = "vlan-";
    static constexpr auto     cNumAllocations        = 8 * cMaxNumConcurrentItems;
    static constexpr auto     cResolvConfLineLen     = AOS_CONFIG_NETWORKMANAGER_RESOLV_CONF_LINE_LEN;

    Error IsInstanceInNetwork(const String& instanceID, const String& networkID) const;
    Error AddInstanceToCache(const String& instanceID, const String& networkID);
    Error PrepareCNIConfig(const String& instanceID, const String& networkID, const InstanceNetworkConfig& network,
        const aos::InstanceNetworkAllocation& networkParams, cni::NetworkConfigList& net, cni::RuntimeConf& rt,
        Array<StaticString<cHostNameLen>>& hosts) const;
    Error PrepareNetworkConfigList(const String& instanceID, const String& networkID,
        const InstanceNetworkConfig& network, const aos::InstanceNetworkAllocation& networkParams,
        cni::NetworkConfigList& net) const;
    Error PrepareRuntimeConfig(
        const String& instanceID, cni::RuntimeConf& rt, const Array<StaticString<cHostNameLen>>& hosts) const;

    Error CreateBridgePluginConfig(const String& networkID, const aos::InstanceNetworkAllocation& networkParams,
        cni::BridgePluginConf& config) const;
    Error CreateFirewallPluginConfig(const String& instanceID, const InstanceNetworkConfig& network,
        const aos::InstanceNetworkAllocation& networkParams, cni::FirewallPluginConf& config) const;
    Error CreateBandwidthPluginConfig(const InstanceNetworkConfig& network, cni::BandwidthNetConf& config) const;
    Error CreateDNSPluginConfig(
        const String& networkID, const aos::InstanceNetworkAllocation& networkParams, cni::DNSPluginConf& config) const;
    Error UpdateInstanceNetworkCache(
        const String& instanceID, const String& networkID, const Array<StaticString<cHostNameLen>>& hosts);
    Error RemoveInstanceFromCache(const String& instanceID, const String& networkID);
    Error ClearNetwork(const NetworkInfo& networkInfo);
    Error PrepareUpdateItemNetworkParams(const InstanceNetworkConfig& instanceNetworkParameters,
        const String& networkID, UpdateItemNetworkParams& serviceData) const;
    Error PrepareHosts(const String& instanceID, const String& networkID, const InstanceNetworkConfig& network,
        Array<StaticString<cHostNameLen>>& hosts) const;
    Error IsHostnameExist(const InstanceCache& instanceCache, const Array<StaticString<cHostNameLen>>& hosts) const;
    Error PushHostWithDomain(
        const String& host, const String& networkID, Array<StaticString<cHostNameLen>>& hosts) const;
    Error CreateHostsFile(const String& networkID, const String& instanceIP, const InstanceNetworkConfig& network,
        const String& hostsFilePath) const;
    Error WriteHost(const Host& host, int fd) const;
    Error WriteHosts(const Array<SharedPtr<Host>>& hosts, int fd) const;
    Error WriteHosts(const Array<Host>& hosts, int fd) const;
    Error WriteHostsFile(
        const String& filePath, const Array<SharedPtr<Host>>& hosts, const Array<Host>& additionalHosts) const;

    Error CreateResolvConfFile(const String& networkID, const String& resolvConfFilePath,
        const aos::InstanceNetworkAllocation& networkParams, const Array<StaticString<cIPLen>>& dns) const;
    Error WriteResolvConfFile(const String& filePath, const Array<StaticString<cIPLen>>& mainServers,
        const aos::InstanceNetworkAllocation& networkParams) const;

    Error CreateNetwork(const NetworkInfo& network);
    Error DeleteInstanceNetworkConfig(const String& instanceID, const String& networkID);
    Error GenerateIfName(String& ifName, const String& ifPrefix);

    template <typename P>
    Error GenerateUniqueIfName(String& ifName, const String& ifPrefix, P&& isUnique)
    {
        for (auto i = 0; i < cCountRetriesIfNameGen; ++i) {
            if (auto err = GenerateIfName(ifName, ifPrefix); !err.IsNone()) {
                return err;
            }

            if (isUnique(ifName)) {
                return ErrorEnum::eNone;
            }
        }

        return ErrorEnum::eNotFound;
    }

    StorageItf*                                                                            mStorage {};
    cni::CNIItf*                                                                           mCNI {};
    TrafficMonitorItf*                                                                     mNetMonitor {};
    NamespaceManagerItf*                                                                   mNetns {};
    InterfaceManagerItf*                                                                   mNetIf {};
    crypto::RandomItf*                                                                     mRandom {};
    InterfaceFactoryItf*                                                                   mNetIfFactory {};
    aos::networkmanager::NetworkProviderItf*                                               mNetworkProvider {};
    StaticString<cIDLen>                                                                   mNodeID;
    StaticString<cFilePathLen>                                                             mCNINetworkCacheDir;
    NetworkCache                                                                           mRuntimeCache;
    StaticMap<StaticString<cIDLen>, NetworkInfo, cMaxNumOwners>                            mNetworkProviders;
    StaticMap<StaticString<cIDLen>, InstanceNetworkInfo, cMaxNumInstances * cMaxNumOwners> mInstanceNetworkInfos;
    StaticArray<StaticString<cIDLen>, cMaxNumOwners>                                       mPhysicalNetworks;
    StaticAllocator<sizeof(StaticArray<NetworkInfo, cMaxNumOwners>)>                       mNetworkInfosAllocator;
    StaticAllocator<sizeof(StaticArray<InstanceNetworkInfo, cMaxNumInstances>)> mInstanceNetworkInfosAllocator;

    mutable Mutex mMutex;
    StaticAllocator<(sizeof(cni::NetworkConfigList) + sizeof(cni::RuntimeConf) + sizeof(cni::Result)
                        + sizeof(cni::FirewallPluginConf) + sizeof(UpdateItemNetworkParams)
                        + sizeof(aos::InstanceNetworkAllocation) + sizeof(InstanceNetworkInfo))
                * cMaxNumConcurrentItems
            + sizeof(StaticArray<StaticString<cIDLen>, cMaxNumInstances>),
        cNumAllocations>
                                                                                          mAllocator;
    mutable StaticAllocator<(sizeof(Host) * 3) * cMaxNumConcurrentItems, cNumAllocations> mHostAllocator;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
