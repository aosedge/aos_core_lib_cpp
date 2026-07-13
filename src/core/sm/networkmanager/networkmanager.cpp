/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>
#include <core/common/tools/memory.hpp>

#include "networkmanager.hpp"

namespace aos::sm::networkmanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error NetworkManager::Init(StorageItf& storage, BridgeNetworkItf& bridgeNet, FirewallItf& firewall,
    BandwidthItf& bandwidth, DNSNameItf& dnsName, TrafficMonitorItf& netMonitor, NamespaceManagerItf& netns,
    InterfaceManagerItf& netIf, crypto::RandomItf& random, InterfaceFactoryItf& netIfFactory,
    aos::networkmanager::NetworkProviderItf& networkProvider, const String& nodeID)
{
    LOG_DBG() << "Init network manager";

    mStorage         = &storage;
    mBridgeNetwork   = &bridgeNet;
    mFirewall        = &firewall;
    mBandwidth       = &bandwidth;
    mDNSName         = &dnsName;
    mNetMonitor      = &netMonitor;
    mNetns           = &netns;
    mNetIf           = &netIf;
    mRandom          = &random;
    mNetIfFactory    = &netIfFactory;
    mNetworkProvider = &networkProvider;
    mNodeID          = nodeID;

    auto instanceNetworkInfos
        = MakeUnique<StaticArray<InstanceNetworkInfo, cMaxNumInstances>>(&mInstanceNetworkInfosAllocator);

    if (auto err = mStorage->GetInstanceNetworksInfo(*instanceNetworkInfos); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& instanceNetworkInfo : *instanceNetworkInfos) {
        mInstanceNetworkInfos.Set(instanceNetworkInfo.mInstanceID, instanceNetworkInfo);
    }

    auto networkInfos = MakeUnique<StaticArray<NetworkInfo, cMaxNumOwners>>(&mNetworkInfosAllocator);

    if (auto err = mStorage->GetNetworksInfo(*networkInfos); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& networkInfo : *networkInfos) {
        mNetworkProviders.Set(networkInfo.mNetworkID, networkInfo);
    }

    return ErrorEnum::eNone;
}

NetworkManager::~NetworkManager()
{
    mRuntimeCache.Clear();

    for (const auto& networkID : mPhysicalNetworks) {
        auto it = mNetworkProviders.Find(networkID);
        if (it == mNetworkProviders.end()) {
            continue;
        }

        if (auto err = ClearNetwork(it->mSecond); !err.IsNone()) {
            LOG_ERR() << "Can't clear network" << Log::Field(err);
        }
    }
}

Error NetworkManager::Start()
{
    LOG_DBG() << "Start network manager";

    Error err;

    if (err = mFirewall->Start(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupFirewall = DeferRelease(&err, [this](const Error* err) {
        if (!err->IsNone()) {
            if (auto errStop = mFirewall->Stop(); !errStop.IsNone()) {
                LOG_ERR() << "Failed to stop firewall" << Log::Field(errStop);
            }
        }
    });

    if (err = mNetMonitor->Start(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupNetMonitor = DeferRelease(&err, [this](const Error* err) {
        if (!err->IsNone()) {
            if (auto errStop = mNetMonitor->Stop(); !errStop.IsNone()) {
                LOG_ERR() << "Failed to stop traffic monitor" << Log::Field(errStop);
            }
        }
    });

    if (err = RemoveDNSOrphans(); !err.IsNone()) {
        return err;
    }

    if (err = CleanupLeftoverInstances(); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::Stop()
{
    LOG_DBG() << "Stop network manager";

    Error err;

    if (auto errStop = mNetMonitor->Stop(); !errStop.IsNone()) {
        err = AOS_ERROR_WRAP(errStop);
    }

    if (auto errStop = mFirewall->Stop(); !errStop.IsNone() && err.IsNone()) {
        err = AOS_ERROR_WRAP(errStop);
    }

    return err;
}

RetWithError<StaticString<cFilePathLen>> NetworkManager::GetNetnsPath(const String& instanceID) const
{
    LOG_DBG() << "Get network namespace path" << Log::Field("instanceID", instanceID);

    return mNetns->GetNetworkNamespacePath(instanceID);
}

Error NetworkManager::GetSystemTraffic(uint64_t& inputTraffic, uint64_t& outputTraffic) const
{
    LOG_DBG() << "Get system traffic";

    auto err = mNetMonitor->GetSystemTraffic(inputTraffic, outputTraffic);

    return AOS_ERROR_WRAP(err);
}

Error NetworkManager::GetInstanceTraffic(
    const String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic) const
{
    LOG_DBG() << "Get instance traffic" << Log::Field("instanceID", instanceID);

    auto err = mNetMonitor->GetInstanceTraffic(instanceID, inputTraffic, outputTraffic);

    return AOS_ERROR_WRAP(err);
}

Error NetworkManager::SetTrafficPeriod(TrafficPeriod period)
{
    LOG_DBG() << "Set traffic period" << Log::Field("period", period);

    mNetMonitor->SetPeriod(period);

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateInstanceNetwork(
    const String& instanceID, const String& networkID, const InstanceNetworkConfig& instanceNetworkParameters)
{
    LOG_DBG() << "Create instance network" << Log::Field("instanceID", instanceID)
              << Log::Field("networkID", networkID);

    {
        LockGuard lock {mMutex};

        if (mInstanceNetworkInfos.Find(instanceID) != mInstanceNetworkInfos.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        if (auto err = mInstanceNetworkInfos.Emplace(instanceID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    Error err;

    auto rollbackCache = DeferRelease(&instanceID, [this, &err](const String* id) {
        if (!err.IsNone()) {
            LockGuard lock {mMutex};
            mInstanceNetworkInfos.Remove(*id);
        }
    });

    if (err = EnsureNodeNetwork(networkID); !err.IsNone()) {
        return err;
    }

    auto serviceData = MakeUnique<UpdateItemNetworkParams>(&mAllocator);

    if (err = PrepareUpdateItemNetworkParams(instanceNetworkParameters, networkID, *serviceData); !err.IsNone()) {
        return err;
    }

    auto allocatedParams = MakeUnique<aos::InstanceNetworkAllocation>(&mAllocator);

    if (err = mNetworkProvider->AllocateInstanceNetwork(
            instanceNetworkParameters.mInstanceIdent, networkID, mNodeID, *serviceData, *allocatedParams);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto rollbackAllocate = DeferRelease(
        &instanceNetworkParameters.mInstanceIdent, [this, &err](const InstanceIdent* ident) {
            if (!err.IsNone()) {
                if (auto errRelease = mNetworkProvider->ReleaseInstanceNetwork(*ident, mNodeID); !errRelease.IsNone()) {
                    LOG_ERR() << "Failed to release instance network on CM" << Log::Field("instanceIdent", *ident)
                              << Log::Field(errRelease);
                }
            }
        });

    auto info = MakeUnique<InstanceNetworkInfo>(
        &mAllocator, instanceID, networkID, instanceNetworkParameters, *allocatedParams);

    if (err = mStorage->AddInstanceNetworkInfo(*info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    {
        LockGuard lock {mMutex};

        mInstanceNetworkInfos.Set(instanceID, *info);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::StartInstanceNetwork(const String& instanceID, const String& networkID)
{
    LOG_DBG() << "Start instance network" << Log::Field("instanceID", instanceID) << Log::Field("networkID", networkID);

    auto cachedInfo = MakeUnique<InstanceNetworkInfo>(&mAllocator);

    {
        LockGuard lock {mMutex};

        auto itInfo = mInstanceNetworkInfos.Find(instanceID);
        if (itInfo == mInstanceNetworkInfos.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance network info not found"));
        }

        if (itInfo->mSecond.mNetworkID != networkID) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "networkID mismatch"));
        }

        *cachedInfo = itInfo->mSecond;
    }

    if (auto errCache = AddInstanceToCache(instanceID, networkID); !errCache.IsNone()) {
        return errCache;
    }

    Error err;

    auto cleanupCache = DeferRelease(&instanceID, [this, networkID, &err](const String* instanceID) {
        if (!err.IsNone()) {
            if (auto errRemove = RemoveInstanceFromCache(*instanceID, networkID); !errRemove.IsNone()) {
                LOG_ERR() << "Failed to remove instance from cache" << Log::Field("instanceID", *instanceID)
                          << Log::Field("networkID", networkID) << Log::Field(errRemove);
            }
        }
    });

    if (err = EnsureNodeNetworkPhysical(networkID); !err.IsNone()) {
        return err;
    }

    err = AddInstanceToNetwork(instanceID, networkID, cachedInfo->mNetworkConfig, cachedInfo->mAllocatedParams);

    return err;
}

Error NetworkManager::GetResolvServers(const String& instanceID, Array<StaticString<cIPLen>>& servers) const
{
    StaticString<cIDLen> networkID;
    StaticString<cIPLen> bridgeIP;
    auto                 dns = MakeUnique<StaticArray<StaticString<cIPLen>, cMaxNumDNSServers>>(&mResolvHostsAllocator);

    {
        LockGuard lock {mMutex};

        auto it = mInstanceNetworkInfos.Find(instanceID);
        if (it == mInstanceNetworkInfos.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance network info not found"));
        }

        networkID = it->mSecond.mNetworkID;
        *dns      = it->mSecond.mAllocatedParams.mDNSServers;

        if (auto np = mNetworkProviders.Find(networkID); np != mNetworkProviders.end()) {
            bridgeIP = np->mSecond.mIP;
        }
    }

    // Per-bridge dnsmasq listens on the bridge IP - make it the primary resolver.
    if (!bridgeIP.IsEmpty()) {
        if (auto err = servers.PushBack(bridgeIP); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& server : *dns) {
        if (servers.Find(server) == servers.end()) {
            if (auto err = servers.PushBack(server); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    if (servers.IsEmpty()) {
        if (auto err = servers.EmplaceBack("8.8.8.8"); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::GetHosts(const String& instanceID, Array<Host>& hosts) const
{
    StaticString<cIDLen>       networkID;
    StaticString<cIPLen>       instanceIP;
    StaticString<cHostNameLen> hostname;
    auto                       customHosts = MakeUnique<StaticArray<Host, cMaxNumHosts>>(&mResolvHostsAllocator);

    {
        LockGuard lock {mMutex};

        auto it = mInstanceNetworkInfos.Find(instanceID);
        if (it == mInstanceNetworkInfos.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "instance network info not found"));
        }

        networkID    = it->mSecond.mNetworkID;
        instanceIP   = it->mSecond.mAllocatedParams.mIP;
        hostname     = it->mSecond.mNetworkConfig.mHostname;
        *customHosts = it->mSecond.mNetworkConfig.mHosts;
    }

    if (auto err = hosts.EmplaceBack("127.0.0.1", "localhost"); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = hosts.EmplaceBack("::1", "localhost ip6-localhost ip6-loopback"); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<cHostNameLen> ownHosts {networkID};

    if (!hostname.IsEmpty()) {
        ownHosts.Append(" ").Append(hostname);
    }

    if (auto err = hosts.EmplaceBack(instanceIP, ownHosts); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& host : *customHosts) {
        if (auto err = hosts.PushBack(host); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::StopInstanceNetwork(const String& instanceID, const String& networkID)
{
    LOG_DBG() << "Stop instance network" << Log::Field("instanceID", instanceID) << Log::Field("networkID", networkID);

    {
        LockGuard lock {mMutex};

        auto itInfo = mInstanceNetworkInfos.Find(instanceID);
        if (itInfo != mInstanceNetworkInfos.end() && itInfo->mSecond.mNetworkID != networkID) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "networkID mismatch"));
        }
    }

    if (auto err = IsInstanceInNetwork(instanceID, networkID); !err.IsNone()) {
        LOG_DBG() << "Instance not found in network";

        return ErrorEnum::eNone;
    }

    Error err;

    if (auto errStop = mNetMonitor->StopInstanceMonitoring(instanceID); !errStop.IsNone()) {
        if (err.IsNone()) {
            err = errStop;
        }
    }

    if (auto errDelete = DeleteInstanceNetworkConfig(instanceID, networkID); !errDelete.IsNone()) {
        if (err.IsNone()) {
            err = errDelete;
        }
    }

    if (auto errRemove = RemoveInstanceFromCache(instanceID, networkID); !errRemove.IsNone() && err.IsNone()) {
        err = errRemove;
    }

    {
        LockGuard lock {mMutex};

        auto network = mRuntimeCache.Find(networkID);
        if (network != mRuntimeCache.end() && !network->mSecond.IsEmpty()) {
            return err;
        }

        auto it = mNetworkProviders.Find(networkID);
        if (it != mNetworkProviders.end()) {
            LOG_DBG() << "Last running instance on network, clearing physical network"
                      << Log::Field("networkID", networkID);

            if (auto errClear = ClearNetwork(it->mSecond); !errClear.IsNone()) {
                LOG_WRN() << "Failed to clear network" << Log::Field("networkID", networkID) << Log::Field(errClear);
            } else {
                mPhysicalNetworks.Remove(networkID);
            }
        }
    }

    return err;
}

Error NetworkManager::ReleaseInstanceNetwork(const String& instanceID, const String& networkID)
{
    LOG_DBG() << "Release instance network" << Log::Field("instanceID", instanceID)
              << Log::Field("networkID", networkID);

    InstanceIdent instanceIdent;
    bool          found = false;

    {
        LockGuard lock {mMutex};

        auto network = mRuntimeCache.Find(networkID);
        if (network != mRuntimeCache.end() && network->mSecond.Find(instanceID) != network->mSecond.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "instance is still running, call Stop first"));
        }

        auto itInfo = mInstanceNetworkInfos.Find(instanceID);
        if (itInfo != mInstanceNetworkInfos.end()) {
            if (itInfo->mSecond.mNetworkID != networkID) {
                return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "networkID mismatch"));
            }

            instanceIdent = itInfo->mSecond.mNetworkConfig.mInstanceIdent;
            found         = true;
            mInstanceNetworkInfos.Remove(instanceID);
        }
    }

    if (auto err = mStorage->RemoveInstanceNetworkInfo(instanceID); !err.IsNone()) {
        LOG_WRN() << "Failed to remove instance network info" << Log::Field("instanceID", instanceID)
                  << Log::Field(err);
    }

    if (found) {
        if (auto err = mNetworkProvider->ReleaseInstanceNetwork(instanceIdent, mNodeID); !err.IsNone()) {
            LOG_WRN() << "Failed to release instance network on CM" << Log::Field("instanceID", instanceID)
                      << Log::Field(err);
        }
    }

    {
        LockGuard lock {mMutex};

        for (const auto& info : mInstanceNetworkInfos) {
            if (info.mSecond.mNetworkID == networkID) {
                return ErrorEnum::eNone;
            }
        }

        auto runtimeNetwork = mRuntimeCache.Find(networkID);
        if (runtimeNetwork != mRuntimeCache.end() && !runtimeNetwork->mSecond.IsEmpty()) {
            return ErrorEnum::eNone;
        }

        mNetworkProviders.Remove(networkID);
    }

    if (auto err = mStorage->RemoveNetworkInfo(networkID); !err.IsNone()) {
        LOG_WRN() << "Failed to remove network info from storage" << Log::Field("networkID", networkID)
                  << Log::Field(err);
    }

    if (auto err = mNetworkProvider->ReleaseNodeNetwork(networkID, mNodeID); !err.IsNone()) {
        LOG_WRN() << "Failed to release node network on CM" << Log::Field("networkID", networkID) << Log::Field(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareUpdateItemNetworkParams(
    const InstanceNetworkConfig& params, const String& networkID, UpdateItemNetworkParams& serviceData) const
{
    serviceData.mExposedPorts       = params.mExposedPorts;
    serviceData.mAllowedConnections = params.mAllowedConnections;

    if (!params.mHostname.IsEmpty()) {
        if (auto err = serviceData.mHosts.PushBack(params.mHostname); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (!params.mInstanceIdent.mItemID.IsEmpty() && !params.mInstanceIdent.mSubjectID.IsEmpty()) {
        StaticString<cHostNameLen> host;

        host.Format("%d.%s.%s", params.mInstanceIdent.mInstance, params.mInstanceIdent.mSubjectID.CStr(),
            params.mInstanceIdent.mItemID.CStr());

        if (auto err = serviceData.mHosts.PushBack(host); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        host.Format("%d.%s.%s.%s", params.mInstanceIdent.mInstance, params.mInstanceIdent.mSubjectID.CStr(),
            params.mInstanceIdent.mItemID.CStr(), networkID.CStr());

        if (auto err = serviceData.mHosts.PushBack(host); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (params.mInstanceIdent.mInstance == 0) {
            host.Format("%s.%s", params.mInstanceIdent.mSubjectID.CStr(), params.mInstanceIdent.mItemID.CStr());

            if (auto err = serviceData.mHosts.PushBack(host); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            host.Format("%s.%s.%s", params.mInstanceIdent.mSubjectID.CStr(), params.mInstanceIdent.mItemID.CStr(),
                networkID.CStr());

            if (auto err = serviceData.mHosts.PushBack(host); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error NetworkManager::EnsureNodeNetwork(const String& networkID)
{
    {
        LockGuard lock {mMutex};

        if (mNetworkProviders.Find(networkID) != mNetworkProviders.end()) {
            return ErrorEnum::eNone;
        }
    }

    LOG_DBG() << "Node network not found, requesting from CM" << Log::Field("networkID", networkID);

    aos::NetworkParams nodeNetParams;

    if (auto err = mNetworkProvider->GetNodeNetworkParams(networkID, mNodeID, nodeNetParams); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LockGuard lock {mMutex};

    if (mNetworkProviders.Find(networkID) != mNetworkProviders.end()) {
        return ErrorEnum::eNone;
    }

    StaticString<cInterfaceLen> vlanIfName;

    if (auto err = GenerateUniqueIfName(vlanIfName, cVlanIfPrefix,
            [this](const String& ifName) {
                return mNetworkProviders.FindIf([&](const auto& network) {
                    return network.mSecond.mVlanIfName == ifName;
                }) == mNetworkProviders.end();
            });
        !err.IsNone()) {
        return err;
    }

    StaticString<cInterfaceLen> bridgeIfName;

    if (auto err = GenerateUniqueIfName(bridgeIfName, cBridgePrefix,
            [this](const String& ifName) {
                return mNetworkProviders.FindIf([&](const auto& network) {
                    return network.mSecond.mBridgeIfName == ifName;
                }) == mNetworkProviders.end();
            });
        !err.IsNone()) {
        return err;
    }

    NetworkInfo networkInfo;
    networkInfo.mNetworkID    = nodeNetParams.mNetworkID;
    networkInfo.mSubnet       = nodeNetParams.mSubnet;
    networkInfo.mIP           = nodeNetParams.mIP;
    networkInfo.mVlanID       = nodeNetParams.mVlanID;
    networkInfo.mVlanIfName   = vlanIfName;
    networkInfo.mBridgeIfName = bridgeIfName;

    if (auto err = mStorage->AddNetworkInfo(networkInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNetworkProviders.Set(networkID, networkInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::AddInstanceToNetwork(const String& instanceID, const String& networkID,
    const InstanceNetworkConfig& networkConfig, const aos::InstanceNetworkAllocation& networkParams)
{
    LOG_DBG() << "Add instance to network" << Log::Field("instanceID", instanceID)
              << Log::Field("networkID", networkID);

    Error err;

    if (err = mNetns->CreateNetworkNamespace(instanceID); !err.IsNone()) {
        return err;
    }

    auto cleanupNetworkNamespace = DeferRelease(&instanceID, [this, &err](const String* instanceID) {
        if (!err.IsNone()) {
            if (auto errDelNetNs = mNetns->DeleteNetworkNamespace(*instanceID); !errDelNetNs.IsNone()) {
                LOG_ERR() << "Failed to delete network namespace" << Log::Field("instanceID", *instanceID)
                          << Log::Field(errDelNetNs);
            }
        }
    });

    auto hosts = MakeUnique<StaticArray<StaticString<cHostNameLen>, cMaxNumHosts>>(&mAllocator);

    if (err = PrepareHosts(instanceID, networkID, networkConfig, *hosts); !err.IsNone()) {
        return err;
    }

    StaticString<cFilePathLen> netNSPath;

    Tie(netNSPath, err) = GetNetnsPath(instanceID);
    if (!err.IsNone()) {
        return err;
    }

    auto bridgeParams = MakeUnique<BridgeParams>(&mAllocator);

    if (err = PrepareBridgeParams(networkID, networkParams, *bridgeParams); !err.IsNone()) {
        return err;
    }

    bridgeParams->mNetNSPath = netNSPath;

    BridgeAttachResult attachResult;

    if (err = mBridgeNetwork->Attach(instanceID, *bridgeParams, attachResult); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupBridge = DeferRelease(&instanceID, [this, &bridgeParams, &err](const String* id) {
        if (!err.IsNone()) {
            if (auto errDetach = mBridgeNetwork->Detach(*id, bridgeParams->mBridgeIfName); !errDetach.IsNone()) {
                LOG_ERR() << "Failed to detach bridge" << Log::Field("instanceID", *id) << Log::Field(errDetach);
            }
        }
    });

    auto firewallParams = MakeUnique<InstanceFirewallParams>(&mAllocator);

    if (err = PrepareInstanceFirewallParams(networkConfig, networkParams, *firewallParams); !err.IsNone()) {
        return err;
    }

    if (err = mFirewall->AddInstance(instanceID, *firewallParams); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupFirewall = DeferRelease(&instanceID, [this, &err](const String* id) {
        if (!err.IsNone()) {
            if (auto errRemove = mFirewall->RemoveInstance(*id); !errRemove.IsNone()) {
                LOG_ERR() << "Failed to remove firewall instance" << Log::Field("instanceID", *id)
                          << Log::Field(errRemove);
            }
        }
    });

    auto bandwidthParams = MakeUnique<BandwidthParams>(&mAllocator);

    if (err = PrepareBandwidthParams(networkConfig, *bandwidthParams); !err.IsNone()) {
        return err;
    }

    if (err = mBandwidth->Apply(attachResult.mHostIfName, *bandwidthParams); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupBandwidth = DeferRelease(&attachResult.mHostIfName, [this, &err](const String* ifName) {
        if (!err.IsNone()) {
            if (auto errClear = mBandwidth->Clear(*ifName); !errClear.IsNone()) {
                LOG_ERR() << "Failed to clear bandwidth" << Log::Field("ifName", *ifName) << Log::Field(errClear);
            }
        }
    });

    DNSServerItf* dnsServer = nullptr;

    {
        LockGuard lock {mMutex};

        auto it = mDNSServers.Find(networkID);
        if (it == mDNSServers.end()) {
            err = AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "DNS server not found"));

            return err;
        }

        dnsServer = it->mSecond;
    }

    auto dnsParams = MakeUnique<DNSAliasesParams>(&mAllocator);

    if (err = PrepareDNSAliasesParams(networkParams, *hosts, *dnsParams); !err.IsNone()) {
        return err;
    }

    if (err = dnsServer->AddHost(instanceID, *dnsParams); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupDNSHost = DeferRelease(&instanceID, [dnsServer, &err](const String* id) {
        if (!err.IsNone()) {
            if (auto errRemove = dnsServer->RemoveHost(*id); !errRemove.IsNone()) {
                LOG_ERR() << "Failed to remove DNS host" << Log::Field("instanceID", *id) << Log::Field(errRemove);
            }
        }
    });

    if (err = mNetMonitor->StartInstanceMonitoring(
            instanceID, networkParams.mIP, networkConfig.mDownloadLimit, networkConfig.mUploadLimit);
        !err.IsNone()) {

        return AOS_ERROR_WRAP(err);
    }

    auto cleanupMonitoring = DeferRelease(&instanceID, [this, &err](const String* id) {
        if (!err.IsNone()) {
            if (auto errStop = mNetMonitor->StopInstanceMonitoring(*id); !errStop.IsNone()) {
                LOG_ERR() << "Failed to stop instance monitoring on rollback" << Log::Field("instanceID", *id)
                          << Log::Field(errStop);
            }
        }
    });

    // resolv.conf / hosts are no longer written here; the caller fetches the
    // data via GetResolvServers/GetHosts and writes the files at its own paths.

    if (err = UpdateInstanceNetworkCache(instanceID, networkID, *hosts); !err.IsNone()) {
        return err;
    }

    auto info = MakeUnique<InstanceNetworkInfo>(&mAllocator);

    {
        LockGuard lock {mMutex};

        auto it = mInstanceNetworkInfos.Find(instanceID);
        if (it == mInstanceNetworkInfos.end()) {
            err = AOS_ERROR_WRAP(ErrorEnum::eNotFound);
            return err;
        }

        *info = it->mSecond;
    }

    info->mHostIfName = attachResult.mHostIfName;

    if (err = mStorage->UpdateInstanceNetworkInfo(*info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    {
        LockGuard lock {mMutex};

        if (auto it = mInstanceNetworkInfos.Find(instanceID); it != mInstanceNetworkInfos.end()) {
            it->mSecond.mHostIfName = attachResult.mHostIfName;
        }
    }

    LOG_DBG() << "Instance added to network" << Log::Field("instanceID", instanceID)
              << Log::Field("networkID", networkID);

    return ErrorEnum::eNone;
}

Error NetworkManager::EnsureNodeNetworkPhysical(const String& networkID)
{
    LockGuard lock {mMutex};

    if (mPhysicalNetworks.Find(networkID) != mPhysicalNetworks.end()) {
        return ErrorEnum::eNone;
    }

    auto it = mNetworkProviders.Find(networkID);
    if (it == mNetworkProviders.end()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "network info not found"));
    }

    if (auto err = CreateNetwork(it->mSecond); !err.IsNone()) {
        return err;
    }

    if (auto err = mPhysicalNetworks.PushBack(networkID); !err.IsNone()) {
        ClearNetwork(it->mSecond);

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::DeleteInstanceNetworkConfig(const String& instanceID, const String& networkID)
{
    StaticString<cInterfaceLen> bridgeIfName;
    StaticString<cInterfaceLen> hostIfName;
    DNSServerItf*               dnsServer = nullptr;

    {
        LockGuard lock {mMutex};

        if (auto it = mNetworkProviders.Find(networkID); it != mNetworkProviders.end()) {
            bridgeIfName = it->mSecond.mBridgeIfName;
        } else {
            LOG_WRN() << "Network provider not found for cleanup" << Log::Field("networkID", networkID);
        }

        if (auto it = mDNSServers.Find(networkID); it != mDNSServers.end()) {
            dnsServer = it->mSecond;
        }

        if (auto it = mInstanceNetworkInfos.Find(instanceID); it != mInstanceNetworkInfos.end()) {
            hostIfName = it->mSecond.mHostIfName;
        } else {
            LOG_WRN() << "Instance network info not found for cleanup" << Log::Field("instanceID", instanceID);
        }
    }

    Error err;

    if (!hostIfName.IsEmpty()) {
        if (dnsServer != nullptr) {
            if (auto errRemove = dnsServer->RemoveHost(instanceID); !errRemove.IsNone() && err.IsNone()) {
                err = AOS_ERROR_WRAP(errRemove);
            }
        } else {
            LOG_WRN() << "DNS server not found for cleanup" << Log::Field("instanceID", instanceID)
                      << Log::Field("networkID", networkID);
        }

        if (auto errClear = mBandwidth->Clear(hostIfName); !errClear.IsNone() && err.IsNone()) {
            err = AOS_ERROR_WRAP(errClear);
        }

        if (auto errRemove = mFirewall->RemoveInstance(instanceID); !errRemove.IsNone() && err.IsNone()) {
            err = AOS_ERROR_WRAP(errRemove);
        }

        if (!bridgeIfName.IsEmpty()) {
            if (auto errDetach = mBridgeNetwork->Detach(instanceID, bridgeIfName);
                !errDetach.IsNone() && err.IsNone()) {
                err = AOS_ERROR_WRAP(errDetach);
            }
        }
    } else {
        LOG_DBG() << "Instance was never started, skipping itf cleanup" << Log::Field("instanceID", instanceID);
    }

    if (auto errDel = mNetns->DeleteNetworkNamespace(instanceID); !errDel.IsNone() && err.IsNone()) {
        err = errDel;
    }

    if (err.IsNone() && !hostIfName.IsEmpty()) {
        auto info        = MakeUnique<InstanceNetworkInfo>(&mAllocator);
        bool needPersist = false;

        {
            LockGuard lock {mMutex};

            if (auto it = mInstanceNetworkInfos.Find(instanceID); it != mInstanceNetworkInfos.end()) {
                *info = it->mSecond;
                info->mHostIfName.Clear();
                needPersist = true;
            }
        }

        if (needPersist) {
            if (auto errUpd = mStorage->UpdateInstanceNetworkInfo(*info); !errUpd.IsNone()) {
                LOG_ERR() << "Failed to clear host interface in storage" << Log::Field("instanceID", instanceID)
                          << Log::Field(errUpd);

                err = AOS_ERROR_WRAP(errUpd);
            } else {
                LockGuard lock {mMutex};

                if (auto it = mInstanceNetworkInfos.Find(instanceID); it != mInstanceNetworkInfos.end()) {
                    it->mSecond.mHostIfName.Clear();
                }
            }
        }
    }

    return err;
}

Error NetworkManager::IsInstanceInNetwork(const String& instanceID, const String& networkID) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Check if instance is in network" << Log::Field("instanceID", instanceID)
              << Log::Field("networkID", networkID);

    auto network = mRuntimeCache.Find(networkID);
    if (network == mRuntimeCache.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    LOG_DBG() << "Network data: " << network->mSecond.Size();

    if (auto instance = network->mSecond.Find(instanceID); instance == network->mSecond.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::UpdateInstanceNetworkCache(
    const String& instanceID, const String& networkID, const Array<StaticString<cHostNameLen>>& hosts)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Update instance network cache" << Log::Field("instanceID", instanceID)
              << Log::Field("networkID", networkID);

    auto network = mRuntimeCache.Find(networkID);
    if (network == mRuntimeCache.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto instance = network->mSecond.Find(instanceID);
    if (instance == network->mSecond.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    instance->mSecond = hosts;

    return ErrorEnum::eNone;
}

Error NetworkManager::CleanupLeftoverInstances()
{
    LOG_DBG() << "Cleanup leftover instances";

    struct Entry {
        StaticString<cIDLen> mInstanceID;
        StaticString<cIDLen> mNetworkID;
    };

    auto entries = MakeUnique<StaticArray<Entry, cMaxNumInstances>>(&mAllocator);

    {
        LockGuard lock {mMutex};

        for (const auto& item : mInstanceNetworkInfos) {
            if (auto err = entries->PushBack({item.mFirst, item.mSecond.mNetworkID}); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    // Adopt a DNS handle for each network with leftover instances, so the
    // RemoveHost call inside DeleteInstanceNetworkConfig has a backend to
    // talk to (CreateInstance is idempotent — it adopts a surviving dnsmasq
    // for this networkID or respawns a fresh one).
    for (const auto& entry : *entries) {
        if (auto err = AdoptDNSServer(entry.mNetworkID); !err.IsNone()) {
            LOG_WRN() << "Failed to adopt DNS server for leftover cleanup" << Log::Field("networkID", entry.mNetworkID)
                      << Log::Field(err);
        }
    }

    for (const auto& entry : *entries) {
        if (auto err = DeleteInstanceNetworkConfig(entry.mInstanceID, entry.mNetworkID); !err.IsNone()) {
            LOG_WRN() << "Failed to delete leftover instance network config"
                      << Log::Field("instanceID", entry.mInstanceID) << Log::Field("networkID", entry.mNetworkID)
                      << Log::Field(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::RemoveDNSOrphans()
{
    auto known = MakeUnique<StaticArray<StaticString<cIDLen>, cMaxNumOwners>>(&mAllocator);

    {
        LockGuard lock {mMutex};

        for (const auto& [id, _] : mNetworkProviders) {
            if (auto err = known->PushBack(id); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    if (auto err = mDNSName->RemoveOrphans(*known); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::AdoptDNSServer(const String& networkID)
{
    NetworkInfo networkInfo;

    {
        LockGuard lock {mMutex};

        if (mDNSServers.Find(networkID) != mDNSServers.end()) {
            return ErrorEnum::eNone;
        }

        auto it = mNetworkProviders.Find(networkID);
        if (it == mNetworkProviders.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "network provider not found"));
        }

        networkInfo = it->mSecond;
    }

    DNSServerParams params;

    if (auto err = PrepareDNSServerParams(networkInfo, params); !err.IsNone()) {
        return err;
    }

    DNSServerItf* handle = nullptr;
    Error         err;

    Tie(handle, err) = mDNSName->CreateServer(networkID, params);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LockGuard lock {mMutex};

    if (auto setErr = mDNSServers.Set(networkID, handle); !setErr.IsNone()) {
        return AOS_ERROR_WRAP(setErr);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::AddInstanceToCache(const String& instanceID, const String& networkID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Add instance to cache" << Log::Field("instanceID", instanceID) << Log::Field("networkID", networkID);

    auto network = mRuntimeCache.Find(networkID);
    if (network == mRuntimeCache.end()) {
        if (auto err = mRuntimeCache.Emplace(networkID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        network = mRuntimeCache.Find(networkID);
    }

    if (network->mSecond.Find(instanceID) != network->mSecond.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
    }

    if (auto err = network->mSecond.Set(instanceID, InstanceHosts()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::RemoveInstanceFromCache(const String& instanceID, const String& networkID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Remove instance from cache" << Log::Field("instanceID", instanceID)
              << Log::Field("networkID", networkID);

    auto network = mRuntimeCache.Find(networkID);
    if (network == mRuntimeCache.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = network->mSecond.Remove(instanceID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (network->mSecond.IsEmpty()) {
        if (auto err = mRuntimeCache.Remove(networkID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::ClearNetwork(const NetworkInfo& networkInfo)
{
    LOG_DBG() << "Clear network" << Log::Field("networkID", networkInfo.mNetworkID);

    Error err;

    if (mDNSServers.Find(networkInfo.mNetworkID) != mDNSServers.end()) {
        if (auto errRemove = mDNSName->RemoveServer(networkInfo.mNetworkID); !errRemove.IsNone() && err.IsNone()) {
            err = AOS_ERROR_WRAP(errRemove);
        }

        mDNSServers.Remove(networkInfo.mNetworkID);
    }

    if (auto errMasq = mFirewall->RemoveMasquerade(networkInfo.mSubnet, networkInfo.mBridgeIfName);
        !errMasq.IsNone() && errMasq.Value() != ErrorEnum::eNotFound && err.IsNone()) {
        err = AOS_ERROR_WRAP(errMasq);
    }

    if (!networkInfo.mBridgeIfName.IsEmpty()) {
        if (auto errDel = mNetIf->DeleteLink(networkInfo.mBridgeIfName);
            !errDel.IsNone() && errDel.Value() != ErrorEnum::eNotFound && err.IsNone()) {
            err = AOS_ERROR_WRAP(errDel);
        }
    }

    if (!networkInfo.mVlanIfName.IsEmpty()) {
        if (auto errDel = mNetIf->DeleteLink(networkInfo.mVlanIfName);
            !errDel.IsNone() && errDel.Value() != ErrorEnum::eNotFound && err.IsNone()) {
            err = AOS_ERROR_WRAP(errDel);
        }
    }

    return err;
}

Error NetworkManager::PrepareHosts(const String& instanceID, const String& networkID,
    const InstanceNetworkConfig& network, Array<StaticString<cHostNameLen>>& hosts) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Prepare hosts" << Log::Field("networkID", networkID);

    auto networkData = mRuntimeCache.Find(networkID);
    if (networkData == mRuntimeCache.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto instanceData = networkData->mSecond.Find(instanceID);
    if (instanceData == networkData->mSecond.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    for (const auto& host : network.mAliases) {
        if (auto err = PushHostWithDomain(host, networkID, hosts); !err.IsNone()) {
            return err;
        }
    }

    if (!network.mHostname.IsEmpty()) {
        if (auto err = PushHostWithDomain(network.mHostname, networkID, hosts); !err.IsNone()) {
            return err;
        }
    }

    if (!network.mInstanceIdent.mItemID.IsEmpty() && !network.mInstanceIdent.mSubjectID.IsEmpty()) {
        StaticString<cHostNameLen> host;

        host.Format("%d.%s.%s", network.mInstanceIdent.mInstance, network.mInstanceIdent.mSubjectID.CStr(),
            network.mInstanceIdent.mItemID.CStr());

        if (auto err = PushHostWithDomain(host, networkID, hosts); !err.IsNone()) {
            return err;
        }

        if (network.mInstanceIdent.mInstance == 0) {
            host.Format("%s.%s", network.mInstanceIdent.mSubjectID.CStr(), network.mInstanceIdent.mItemID.CStr());

            if (auto err = PushHostWithDomain(host, networkID, hosts); !err.IsNone()) {
                return err;
            }
        }
    }

    return IsHostnameExist(networkData->mSecond, hosts);
}

Error NetworkManager::PushHostWithDomain(
    const String& host, const String& networkID, Array<StaticString<cHostNameLen>>& hosts) const
{
    if (auto ret = hosts.Find(host); ret != hosts.end()) {
        return ErrorEnum::eAlreadyExist;
    }

    if (auto err = hosts.PushBack(host); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (host.Find('.') != host.end()) {
        StaticString<cHostNameLen> withDomain;

        if (auto err = withDomain.Format("%s.%s", host.CStr(), networkID.CStr()); !err.IsNone()) {
            return err;
        }

        if (hosts.Find(withDomain) != hosts.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        return AOS_ERROR_WRAP(hosts.PushBack(withDomain));
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::IsHostnameExist(
    const InstanceCache& instanceCache, const Array<StaticString<cHostNameLen>>& hosts) const
{
    for (const auto& host : hosts) {
        for (const auto& instance : instanceCache) {
            if (instance.mSecond.Find(host) != instance.mSecond.end()) {
                return ErrorEnum::eAlreadyExist;
            }
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareBridgeParams(
    const String& networkID, const aos::InstanceNetworkAllocation& networkParams, BridgeParams& params) const
{
    StaticString<cInterfaceLen> bridgeIfName;
    StaticString<cIPLen>        gateway;

    {
        LockGuard lock {mMutex};

        auto it = mNetworkProviders.Find(networkID);
        if (it == mNetworkProviders.end()) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        bridgeIfName = it->mSecond.mBridgeIfName;
        gateway      = it->mSecond.mIP;
    }

    params.mBridgeIfName    = bridgeIfName;
    params.mContainerIfName = cDefaultContainerIfName;
    params.mGateway         = gateway;
    params.mHairpin         = true;

    if (auto slash = networkParams.mSubnet.Find('/'); slash != networkParams.mSubnet.end()) {
        if (auto err = params.mIPWithMask.Format("%s%s", networkParams.mIP.CStr(),
                networkParams.mSubnet.CStr() + (slash - networkParams.mSubnet.begin()));
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        params.mIPWithMask = networkParams.mIP;
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareInstanceFirewallParams(const InstanceNetworkConfig& networkConfig,
    const aos::InstanceNetworkAllocation& networkParams, InstanceFirewallParams& params) const
{
    params.mIP          = networkParams.mIP;
    params.mSubnet      = networkParams.mSubnet;
    params.mAllowPublic = true;

    StaticArray<StaticString<cPortLen>, cMaxExposedPort> portConfig;

    for (const auto& port : networkConfig.mExposedPorts) {
        if (auto err = port.Split(portConfig, '/'); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (portConfig.IsEmpty()) {
            return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
        }

        if (auto err = params.mInput.PushBack({portConfig[0], portConfig.Size() > 1 ? portConfig[1] : String("tcp")});
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& rule : networkParams.mFirewallRules) {
        if (auto err = params.mOutput.PushBack({rule.mDstIP, rule.mDstPort, rule.mProto, rule.mSrcIP}); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareBandwidthParams(const InstanceNetworkConfig& networkConfig, BandwidthParams& params) const
{
    if (networkConfig.mIngressKbit > 0) {
        params.mIngressRate  = networkConfig.mIngressKbit * 1000;
        params.mIngressBurst = cBurstLen;
    }

    if (networkConfig.mEgressKbit > 0) {
        params.mEgressRate  = networkConfig.mEgressKbit * 1000;
        params.mEgressBurst = cBurstLen;
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareDNSAliasesParams(const aos::InstanceNetworkAllocation& networkParams,
    const Array<StaticString<cHostNameLen>>& aliases, DNSAliasesParams& params) const
{
    params.mIP = networkParams.mIP;

    if (auto err = params.mAliases.Assign(aliases); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareDNSServerParams(const NetworkInfo& network, DNSServerParams& params) const
{
    params.mBridgeIP     = network.mIP;
    params.mBridgeIfName = network.mBridgeIfName;

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateNetwork(const NetworkInfo& network)
{
    LOG_DBG() << "Create network" << Log::Field("networkID", network.mNetworkID)
              << Log::Field("subnet", network.mSubnet) << Log::Field("ip", network.mIP)
              << Log::Field("vlanID", network.mVlanID) << Log::Field("bridgeIfName", network.mBridgeIfName)
              << Log::Field("vlanIfName", network.mVlanIfName);

    Error err;

    if (err = mNetIfFactory->CreateBridge(network.mBridgeIfName, network.mIP, network.mSubnet); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupBridge = DeferRelease(&network, [this, &err](const NetworkInfo* network) {
        if (!err.IsNone()) {
            mNetIf->DeleteLink(network->mBridgeIfName);
        }
    });

    // Create the vlan already enslaved to the bridge (master) in one operation,
    // avoiding a separate SetMasterLink round-trip.
    if (err = mNetIfFactory->CreateVlan(network.mVlanIfName, network.mVlanID, network.mBridgeIfName); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupVlan = DeferRelease(&network, [this, &err](const NetworkInfo* network) {
        if (!err.IsNone()) {
            mNetIf->DeleteLink(network->mVlanIfName);
        }
    });

    // Masquerade is a per-network property (one rule per subnet/bridge), so it
    // is installed here on network creation rather than per instance.
    if (err = mFirewall->AddMasquerade(network.mSubnet, network.mBridgeIfName); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupMasquerade = DeferRelease(&network, [this, &err](const NetworkInfo* network) {
        if (!err.IsNone()) {
            if (auto errMasq = mFirewall->RemoveMasquerade(network->mSubnet, network->mBridgeIfName);
                !errMasq.IsNone()) {
                LOG_ERR() << "Failed to remove masquerade on CreateNetwork rollback"
                          << Log::Field("networkID", network->mNetworkID) << Log::Field(errMasq);
            }
        }
    });

    DNSServerParams dnsParams;

    if (err = PrepareDNSServerParams(network, dnsParams); !err.IsNone()) {
        return err;
    }

    DNSServerItf* dnsServer = nullptr;

    Tie(dnsServer, err) = mDNSName->CreateServer(network.mNetworkID, dnsParams);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupDNSServer = DeferRelease(&network, [this, &err](const NetworkInfo* network) {
        if (!err.IsNone()) {
            if (auto errRemove = mDNSName->RemoveServer(network->mNetworkID); !errRemove.IsNone()) {
                LOG_ERR() << "Failed to remove DNS server on CreateNetwork rollback"
                          << Log::Field("networkID", network->mNetworkID) << Log::Field(errRemove);
            }
        }
    });

    if (err = mDNSServers.Set(network.mNetworkID, dnsServer); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Network created" << Log::Field("networkID", network.mNetworkID);

    return ErrorEnum::eNone;
}

Error NetworkManager::GenerateIfName(String& ifName, const String& ifPrefix)
{
    ifName.Clear();

    ifName.Append(ifPrefix);

    String randomString = String(ifName.Get() + ifPrefix.Size(), ifName.MaxSize() - ifPrefix.Size());

    if (auto err = crypto::GenerateRandomString<cMaxNetworkIDLen / 2>(randomString, *mRandom); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    ifName.Resize(ifName.Size() + randomString.Size());

    return ErrorEnum::eNone;
}

void NetworkManager::OnPendingFirewallUpdate(
    const String& nodeID, const aos::networkmanager::PendingFirewallUpdate& update)
{
    if (nodeID != mNodeID) {
        LOG_WRN() << "Ignoring pending firewall update for different node" << Log::Field("expectedNodeID", mNodeID)
                  << Log::Field("receivedNodeID", nodeID);

        return;
    }

    LOG_DBG() << "Received pending firewall update" << Log::Field("instanceIdent", update.mInstanceIdent)
              << Log::Field("rulesCount", update.mFirewallRules.Size());

    StaticString<cIDLen> instanceID;
    StaticString<cIDLen> networkID;
    bool                 isRunning = false;

    auto networkConfig   = MakeUnique<InstanceNetworkConfig>(&mAllocator);
    auto allocatedParams = MakeUnique<aos::InstanceNetworkAllocation>(&mAllocator);

    StaticString<cInterfaceLen> hostIfName;

    {
        LockGuard lock {mMutex};

        for (const auto& item : mInstanceNetworkInfos) {
            if (item.mSecond.mNetworkConfig.mInstanceIdent == update.mInstanceIdent) {
                instanceID       = item.mFirst;
                networkID        = item.mSecond.mNetworkID;
                *networkConfig   = item.mSecond.mNetworkConfig;
                *allocatedParams = item.mSecond.mAllocatedParams;
                hostIfName       = item.mSecond.mHostIfName;

                auto network = mRuntimeCache.Find(networkID);
                if (network != mRuntimeCache.end()) {
                    isRunning = network->mSecond.Find(instanceID) != network->mSecond.end();
                }

                break;
            }
        }

        if (instanceID.IsEmpty()) {
            LOG_WRN() << "Instance not found for pending firewall update"
                      << Log::Field("instanceIdent", update.mInstanceIdent);

            return;
        }

        allocatedParams->mFirewallRules = update.mFirewallRules;

        auto info = MakeUnique<InstanceNetworkInfo>(
            &mInstanceNetworkInfosAllocator, instanceID, networkID, *networkConfig, *allocatedParams, hostIfName);

        if (auto err = mStorage->UpdateInstanceNetworkInfo(*info); !err.IsNone()) {
            LOG_ERR() << "Failed to update instance network info" << Log::Field("instanceID", instanceID)
                      << Log::Field(err);

            return;
        }

        if (auto it = mInstanceNetworkInfos.Find(instanceID); it != mInstanceNetworkInfos.end()) {
            it->mSecond.mAllocatedParams.mFirewallRules = update.mFirewallRules;
        }
    }

    if (isRunning) {
        if (auto err = UpdateInstanceFirewall(instanceID, networkID, *networkConfig, *allocatedParams); !err.IsNone()) {
            LOG_ERR() << "Failed to update instance firewall" << Log::Field("instanceID", instanceID)
                      << Log::Field(err);
        }
    }
}

void NetworkManager::OnConnect()
{
    LOG_DBG() << "SM connected to CM, synchronizing network state";

    auto instances = MakeUnique<StaticArray<InstanceNetworkStateInfo, cMaxNumInstances>>(&mAllocator);

    {
        LockGuard lock {mMutex};

        for (const auto& [id, info] : mInstanceNetworkInfos) {
            // Only sync running instances (present in runtime cache)
            auto network = mRuntimeCache.Find(info.mNetworkID);
            if (network == mRuntimeCache.end() || network->mSecond.Find(id) == network->mSecond.end()) {
                continue;
            }

            if (auto err = instances->EmplaceBack(info.mNetworkConfig.mInstanceIdent, info.mNetworkID,
                    info.mAllocatedParams.mIP, info.mAllocatedParams.mFirewallRules);
                !err.IsNone()) {
                LOG_ERR() << "Failed to add instance to sync state" << Log::Field(err);

                return;
            }
        }
    }

    if (auto err = mNetworkProvider->SyncNetworkState(mNodeID, *instances); !err.IsNone()) {
        LOG_ERR() << "Failed to sync network state with CM" << Log::Field(err);
    }
}

Error NetworkManager::UpdateInstanceFirewall(const String& instanceID, const String& networkID,
    const InstanceNetworkConfig& networkConfig, const aos::InstanceNetworkAllocation& networkParams)
{
    LOG_DBG() << "Updating instance firewall" << Log::Field("instanceID", instanceID)
              << Log::Field("networkID", networkID);

    auto firewallParams = MakeUnique<InstanceFirewallParams>(&mAllocator);

    if (auto err = PrepareInstanceFirewallParams(networkConfig, networkParams, *firewallParams); !err.IsNone()) {
        return err;
    }

    if (auto err = mFirewall->UpdateInstance(instanceID, *firewallParams); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::networkmanager
