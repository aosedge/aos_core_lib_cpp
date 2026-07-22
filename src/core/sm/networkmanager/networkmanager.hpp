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

#include "itf/bandwidth.hpp"
#include "itf/bridgenetwork.hpp"
#include "itf/dnsname.hpp"
#include "itf/firewall.hpp"
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
     * @param bridgeNet bridge network interface.
     * @param firewall firewall interface.
     * @param bandwidth bandwidth interface.
     * @param dnsName DNS name interface.
     * @param netMonitor traffic monitor.
     * @param netns namespace manager.
     * @param netIf network interface manager.
     * @return Error.
     */
    Error Init(StorageItf& storage, BridgeNetworkItf& bridgeNet, FirewallItf& firewall, BandwidthItf& bandwidth,
        DNSNameItf& dnsName, TrafficMonitorItf& netMonitor, NamespaceManagerItf& netns, InterfaceManagerItf& netIf,
        crypto::RandomItf& random, InterfaceFactoryItf& netIfFactory,
        aos::networkmanager::NetworkProviderItf& networkProvider, const String& nodeID);

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
     * @return Error.
     */
    Error StartInstanceNetwork(const String& instanceID, const String& networkID) override;

    /**
     * Returns resolver IPs for the instance (caller prefixes each with "nameserver").
     *
     * @param instanceID instance ID.
     * @param[out] servers resolver IP addresses.
     * @return Error.
     */
    Error GetResolvServers(const String& instanceID, Array<StaticString<cIPLen>>& servers) const override;

    /**
     * Returns host entries (IP + hostname) for the instance.
     *
     * @param instanceID instance ID.
     * @param[out] hosts host entries.
     * @return Error.
     */
    Error GetHosts(const String& instanceID, Array<Host>& hosts) const override;

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
     * Opens a batch for start/stop operations.
     *
     * @return Error.
     */
    Error BeginBatch() override;

    /**
     * Flushes the staged batch.
     *
     * @param[out] failedInstanceIDs instances that were not applied.
     * @return Error.
     */
    Error FlushBatch(Array<StaticString<cIDLen>>& failedInstanceIDs) override;

    /**
     * Called when pending firewall rules are resolved for an instance.
     *
     * @param nodeID node ID where the instance resides.
     * @param update pending firewall update.
     */
    void OnPendingFirewallUpdate(
        const String& nodeID, const aos::networkmanager::PendingFirewallUpdate& update) override;

    /**
     * Called when SM successfully connects/reconnects to CM.
     */
    void OnConnect() override;

private:
    using InstanceHosts = StaticArray<StaticString<cHostNameLen>, cMaxNumHosts>;
    using InstanceCache = StaticMap<StaticString<cIDLen>, InstanceHosts, cMaxNumInstances>;
    using NetworkCache  = StaticMap<StaticString<cIDLen>, InstanceCache, cMaxNumOwners>;

    enum class BatchOp { eAdd, eRemove };

    struct BatchEntry {
        StaticString<cIDLen> mInstanceID;
        StaticString<cIDLen> mNetworkID;
        BatchOp              mOp;
    };

    // StartInstanceNetwork keeps its cached InstanceNetworkInfo alive across the nested call to
    // AddInstanceToNetwork, which in turn allocates hosts, bridge/firewall/bandwidth/DNS params and
    // its own InstanceNetworkInfo before returning. That is the largest concurrent footprint of any
    // mAllocator call chain, so it sizes the per-concurrent-item budget below (it dominates the
    // smaller CreateInstanceNetwork and OnPendingFirewallUpdate chains).
    static constexpr auto cMaxOperationAllocatorSize = 2 * sizeof(InstanceNetworkInfo) + sizeof(InstanceHosts)
        + sizeof(BridgeParams) + sizeof(InstanceFirewallParams) + sizeof(BandwidthParams) + sizeof(DNSAliasesParams);

    // Start()/OnConnect() run once, outside the concurrent instance-operation hot path, so their
    // allocations are added rather than multiplied by cMaxNumConcurrentItems: RemoveDNSOrphans' known
    // networks list, CleanupLeftoverInstances' leftover instance/network ID pairs plus the
    // InstanceNetworkInfo DeleteInstanceNetworkConfig allocates while clearing a host interface, and
    // OnConnect's state sync snapshot.
    static constexpr auto cAllocatorSize = cMaxOperationAllocatorSize * cMaxNumConcurrentItems
        + sizeof(StaticArray<StaticString<cIDLen>, cMaxNumOwners>)
        + sizeof(StaticArray<StaticString<cIDLen>, cMaxNumInstances>) * 2 + sizeof(InstanceNetworkInfo)
        + sizeof(StaticArray<InstanceNetworkStateInfo, cMaxNumInstances>);
    static constexpr auto cNumAllocations = 8 * cMaxNumConcurrentItems;

    // GetHosts/GetResolvServers are const methods that instance start/stop pool tasks call
    // concurrently (one call per in-flight instance), each making a single allocation off
    // mResolvHostsAllocator that stays alive for the call's duration. Size it for
    // cMaxNumConcurrentItems concurrent callers instead of just one.
    static constexpr auto cResolvHostsAllocatorSize = cMaxNumConcurrentItems
        * (sizeof(StaticArray<Host, cMaxNumHosts>) + sizeof(StaticArray<StaticString<cIPLen>, cMaxNumDNSServers>));

    static constexpr uint64_t cBurstLen              = 12800;
    static constexpr auto     cMaxExposedPort        = 2;
    static constexpr auto     cCountRetriesIfNameGen = 10;
    static constexpr auto     cMaxNetworkIDLen       = 8;
    static constexpr auto     cBridgePrefix          = "br-";
    static constexpr auto     cVlanIfPrefix          = "vlan-";
    static constexpr auto     cResolvConfLineLen     = AOS_CONFIG_NETWORKMANAGER_RESOLV_CONF_LINE_LEN;

    Error EnsureNodeNetwork(const String& networkID);
    Error EnsureNodeNetworkPhysical(const String& networkID);
    Error UpdateInstanceFirewall(const String& instanceID, const String& networkID,
        const InstanceNetworkConfig& networkConfig, const aos::InstanceNetworkAllocation& networkParams);
    Error AddInstanceToNetwork(const String& instanceID, const String& networkID,
        const InstanceNetworkConfig& networkConfig, const aos::InstanceNetworkAllocation& networkParams);
    Error ReapplyInstancePolicy(const BatchEntry& entry);
    Error IsInstanceInNetwork(const String& instanceID, const String& networkID) const;
    Error AddInstanceToCache(const String& instanceID, const String& networkID);
    Error CleanupLeftoverInstances();
    Error RemoveDNSOrphans();
    Error AdoptDNSServer(const String& networkID);
    Error PrepareBridgeParams(
        const String& networkID, const aos::InstanceNetworkAllocation& networkParams, BridgeParams& params) const;
    Error PrepareInstanceFirewallParams(const InstanceNetworkConfig& networkConfig,
        const aos::InstanceNetworkAllocation& networkParams, InstanceFirewallParams& params) const;
    Error PrepareBandwidthParams(const InstanceNetworkConfig& networkConfig, BandwidthParams& params) const;
    Error PrepareDNSAliasesParams(const aos::InstanceNetworkAllocation& networkParams,
        const Array<StaticString<cHostNameLen>>& aliases, DNSAliasesParams& params) const;
    Error PrepareDNSServerParams(const NetworkInfo& network, DNSServerParams& params) const;
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
    BridgeNetworkItf*                                                                      mBridgeNetwork {};
    FirewallItf*                                                                           mFirewall {};
    BandwidthItf*                                                                          mBandwidth {};
    DNSNameItf*                                                                            mDNSName {};
    TrafficMonitorItf*                                                                     mNetMonitor {};
    NamespaceManagerItf*                                                                   mNetns {};
    InterfaceManagerItf*                                                                   mNetIf {};
    crypto::RandomItf*                                                                     mRandom {};
    InterfaceFactoryItf*                                                                   mNetIfFactory {};
    aos::networkmanager::NetworkProviderItf*                                               mNetworkProvider {};
    StaticString<cIDLen>                                                                   mNodeID;
    NetworkCache                                                                           mRuntimeCache;
    StaticMap<StaticString<cIDLen>, NetworkInfo, cMaxNumOwners>                            mNetworkProviders;
    StaticMap<StaticString<cIDLen>, DNSServerItf*, cMaxNumOwners>                          mDNSServers;
    StaticMap<StaticString<cIDLen>, InstanceNetworkInfo, cMaxNumInstances * cMaxNumOwners> mInstanceNetworkInfos;
    StaticArray<StaticString<cIDLen>, cMaxNumOwners>                                       mPhysicalNetworks;
    bool                                                                                   mBatchMode {false};
    StaticArray<BatchEntry, cMaxNumInstances * cMaxNumOwners>                              mBatchEntries;
    StaticAllocator<sizeof(StaticArray<NetworkInfo, cMaxNumOwners>)>                       mNetworkInfosAllocator;
    StaticAllocator<sizeof(StaticArray<InstanceNetworkInfo, cMaxNumInstances>)> mInstanceNetworkInfosAllocator;

    mutable Mutex                                                                  mMutex;
    StaticAllocator<cAllocatorSize, cNumAllocations>                               mAllocator;
    mutable StaticAllocator<cResolvHostsAllocatorSize, 2 * cMaxNumConcurrentItems> mResolvHostsAllocator;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
