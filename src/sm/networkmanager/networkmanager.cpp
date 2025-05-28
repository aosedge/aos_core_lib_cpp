/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <unistd.h>

#include "aos/common/crypto/utils.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/sm/networkmanager.hpp"

#include "log.hpp"

namespace aos::sm::networkmanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error NetworkManager::Init(StorageItf& storage, cni::CNIItf& cni, TrafficMonitorItf& netMonitor,
    NamespaceManagerItf& netns, InterfaceManagerItf& netIf, crypto::RandomItf& random,
    InterfaceFactoryItf& netIfFactory, const String& workingDir)
{
    LOG_DBG() << "Init network manager";

    mStorage      = &storage;
    mCNI          = &cni;
    mNetMonitor   = &netMonitor;
    mNetns        = &netns;
    mNetIf        = &netIf;
    mRandom       = &random;
    mNetIfFactory = &netIfFactory;

    auto cniDir = fs::JoinPath(workingDir, "cni");

    if (auto err = fs::RemoveAll(cniDir); !err.IsNone()) {
        LOG_ERR() << "Failed to remove cni directory: " << cniDir;
    }

    if (auto err = mCNI->SetConfDir(cniDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mCNINetworkCacheDir = fs::JoinPath(cniDir, "networks");

    auto networkInfos = MakeUnique<StaticArray<NetworkInfo, cMaxNumServiceProviders>>(&mNetworkInfosAllocator);

    if (auto err = mStorage->GetNetworksInfo(*networkInfos); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& networkInfo : *networkInfos) {
        if (auto err = CreateNetwork(networkInfo); !err.IsNone()) {
            return err;
        }

        mNetworkProviders.Set(networkInfo.mNetworkID, networkInfo);
    }

    return ErrorEnum::eNone;
}

NetworkManager::~NetworkManager()
{
    mNetworkData.Clear();
}

Error NetworkManager::Start()
{
    LOG_DBG() << "Start network manager";

    return AOS_ERROR_WRAP(mNetMonitor->Start());
}

Error NetworkManager::Stop()
{
    LOG_DBG() << "Stop network manager";

    return AOS_ERROR_WRAP(mNetMonitor->Stop());
}

Error NetworkManager::UpdateNetworks(const Array<aos::NetworkParameters>& networks)
{
    LOG_DBG() << "Update networks";

    if (auto err = RemoveNetworks(networks); !err.IsNone()) {
        return err;
    }

    LockGuard lock {mMutex};

    auto networkInfo = MakeUnique<NetworkInfo>(&mNetworkInfoAllocator);

    for (const auto& network : networks) {
        if (auto it = mNetworkProviders.Find(network.mNetworkID); it != mNetworkProviders.end()) {
            if (it->mSecond.mIP == network.mIP) {
                continue;
            }
        }

        StaticString<cInterfaceLen> vlanIfName;

        if (auto err = GenerateVlanIfName(vlanIfName); !err.IsNone()) {
            return err;
        }

        networkInfo->mNetworkID  = network.mNetworkID;
        networkInfo->mSubnet     = network.mSubnet;
        networkInfo->mIP         = network.mIP;
        networkInfo->mVlanID     = network.mVlanID;
        networkInfo->mVlanIfName = vlanIfName;

        if (auto err = CreateNetwork(*networkInfo); !err.IsNone()) {
            return err;
        }

        if (auto err = mStorage->AddNetworkInfo(*networkInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = mNetworkProviders.Set(network.mNetworkID, *networkInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::AddInstanceToNetwork(
    const String& instanceID, const String& networkID, const InstanceNetworkParameters& instanceNetworkParameters)
{
    LOG_DBG() << "Add instance to network: instanceID=" << instanceID << ", networkID=" << networkID;

    auto err = IsInstanceInNetwork(instanceID, networkID);
    if (err.IsNone()) {
        return ErrorEnum::eAlreadyExist;
    }

    if (!err.Is(ErrorEnum::eNotFound)) {
        return err;
    }

    if (err = AddInstanceToCache(instanceID, networkID); !err.IsNone()) {
        return err;
    }

    auto cleanupInstanceCache = DeferRelease(&instanceID, [this, networkID, &err](const String* instanceID) {
        if (!err.IsNone()) {
            if (auto errRemoveInstanceFromCache = RemoveInstanceFromCache(*instanceID, networkID);
                !errRemoveInstanceFromCache.IsNone()) {
                LOG_ERR() << "Failed to remove instance from cache: instanceID=" << *instanceID
                          << ", networkID=" << networkID << ", err=" << errRemoveInstanceFromCache;
            }
        }
    });

    if (err = mNetns->CreateNetworkNamespace(instanceID); !err.IsNone()) {
        return err;
    }

    auto cleanupNetworkNamespace = DeferRelease(&instanceID, [this, &err](const String* instanceID) {
        if (!err.IsNone()) {
            if (auto errDelNetNs = mNetns->DeleteNetworkNamespace(*instanceID); !errDelNetNs.IsNone()) {
                LOG_ERR() << "Failed to delete network namespace: instanceID=" << *instanceID
                          << ", err=" << errDelNetNs;
            }
        }
    });

    auto netConfigList = MakeUnique<cni::NetworkConfigList>(&mAllocator);
    auto rtConfig      = MakeUnique<cni::RuntimeConf>(&mAllocator);

    StaticArray<StaticString<cHostNameLen>, cMaxNumHosts> host;

    if (err = PrepareCNIConfig(instanceID, networkID, instanceNetworkParameters, *netConfigList, *rtConfig, host);
        !err.IsNone()) {
        return err;
    }

    auto result = MakeUnique<cni::Result>(&mAllocator);

    if (err = mCNI->AddNetworkList(*netConfigList, *rtConfig, *result); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupCNI = DeferRelease(&instanceID, [this, &netConfigList, &rtConfig, &err](const String* instanceID) {
        if (!err.IsNone()) {
            if (auto errCleanCNI = mCNI->DeleteNetworkList(*netConfigList, *rtConfig); !errCleanCNI.IsNone()) {
                LOG_ERR() << "Failed to delete network list: instanceID=" << *instanceID << ", err=" << errCleanCNI;
            }
        }
    });

    if (err = mNetMonitor->StartInstanceMonitoring(instanceID, instanceNetworkParameters.mNetworkParameters.mIP,
            instanceNetworkParameters.mDownloadLimit, instanceNetworkParameters.mUploadLimit);
        !err.IsNone()) {

        return AOS_ERROR_WRAP(err);
    }

    if (err = CreateHostsFile(networkID, instanceNetworkParameters.mNetworkParameters.mIP, instanceNetworkParameters);
        !err.IsNone()) {
        return err;
    }

    if (err = CreateResolvConfFile(networkID, instanceNetworkParameters, result->mDNSServers); !err.IsNone()) {
        return err;
    }

    if (err = UpdateInstanceNetworkCache(instanceID, networkID, instanceNetworkParameters.mNetworkParameters.mIP, host);
        !err.IsNone()) {
        return err;
    }

    LOG_DBG() << "Instance added to network: instanceID=" << instanceID << ", networkID=" << networkID;

    return ErrorEnum::eNone;
}

Error NetworkManager::RemoveInstanceFromNetwork(const String& instanceID, const String& networkID)
{
    LOG_DBG() << "Remove instance from network: instanceID=" << instanceID << ", networkID=" << networkID;

    if (!mNetworkProviders.Contains(networkID) && !mNetworkData.Contains(networkID)) {
        LOG_WRN() << "Network not found" << Log::Field("networkID", networkID);

        return ErrorEnum::eNone;
    }

    if (auto err = IsInstanceInNetwork(instanceID, networkID); !err.IsNone()) {
        return err;
    }

    if (auto err = mNetMonitor->StopInstanceMonitoring(instanceID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto netConfig = MakeUnique<cni::NetworkConfigList>(&mAllocator);
    auto rtConfig  = MakeUnique<cni::RuntimeConf>(&mAllocator);

    netConfig->mName    = networkID;
    netConfig->mVersion = cni::cVersion;

    rtConfig->mContainerID = instanceID;

    Error err;

    Tie(rtConfig->mNetNS, err) = GetNetnsPath(instanceID);
    if (!err.IsNone()) {
        return err;
    }

    rtConfig->mIfName = cInstanceInterfaceName;

    if (err = mCNI->GetNetworkListCachedConfig(*netConfig, *rtConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = mCNI->DeleteNetworkList(*netConfig, *rtConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = mNetns->DeleteNetworkNamespace(instanceID); !err.IsNone()) {
        return err;
    }

    if (err = RemoveInstanceFromCache(instanceID, networkID); !err.IsNone()) {
        return err;
    }

    LOG_DBG() << "Instance removed from network: instanceID=" << instanceID << ", networkID=" << networkID;

    return ErrorEnum::eNone;
}

RetWithError<StaticString<cFilePathLen>> NetworkManager::GetNetnsPath(const String& instanceID) const
{
    LOG_DBG() << "Get network namespace path: instanceID=" << instanceID;

    return mNetns->GetNetworkNamespacePath(instanceID);
}

Error NetworkManager::GetInstanceIP(const String& instanceID, const String& networkID, String& ip) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get instance IP: instanceID=" << instanceID << ", networkID=" << networkID;

    auto network = mNetworkData.Find(networkID);
    if (network == mNetworkData.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto instance = network->mSecond.Find(instanceID);
    if (instance == network->mSecond.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    ip = instance->mSecond.mIPAddr;

    return ErrorEnum::eNone;
}

Error NetworkManager::GetSystemTraffic(uint64_t& inputTraffic, uint64_t& outputTraffic) const
{
    LOG_DBG() << "Get system traffic";

    return AOS_ERROR_WRAP(mNetMonitor->GetSystemData(inputTraffic, outputTraffic));
}

Error NetworkManager::GetInstanceTraffic(
    const String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic) const
{
    LOG_DBG() << "Get instance traffic: instanceID=" << instanceID;

    return AOS_ERROR_WRAP(mNetMonitor->GetInstanceTraffic(instanceID, inputTraffic, outputTraffic));
}

Error NetworkManager::SetTrafficPeriod(TrafficPeriod period)
{
    LOG_DBG() << "Set traffic period: period=" << period;

    mNetMonitor->SetPeriod(period);

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error NetworkManager::IsInstanceInNetwork(const String& instanceID, const String& networkID) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Check if instance is in network: instanceID=" << instanceID << ", networkID=" << networkID;

    auto network = mNetworkData.Find(networkID);
    if (network == mNetworkData.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto instance = network->mSecond.Find(instanceID); instance == network->mSecond.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::UpdateInstanceNetworkCache(const String& instanceID, const String& networkID,
    const String& instanceIP, const Array<StaticString<cHostNameLen>>& hosts)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Update instance network cache: instanceID=" << instanceID << ", networkID=" << networkID;

    auto network = mNetworkData.Find(networkID);
    if (network == mNetworkData.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto instance = network->mSecond.Find(instanceID);
    if (instance == network->mSecond.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    instance->mSecond.mIPAddr = instanceIP;
    instance->mSecond.mHost   = hosts;

    return ErrorEnum::eNone;
}

Error NetworkManager::AddInstanceToCache(const String& instanceID, const String& networkID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Add instance to cache: instanceID=" << instanceID << ", networkID=" << networkID;

    auto network = mNetworkData.Find(networkID);
    if (network == mNetworkData.end()) {
        if (auto err = mNetworkData.Emplace(networkID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = mNetworkData.Find(networkID)->mSecond.Set(instanceID, NetworkData()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::RemoveInstanceFromCache(const String& instanceID, const String& networkID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Remove instance from cache: instanceID=" << instanceID << ", networkID=" << networkID;

    auto network = mNetworkData.Find(networkID);
    if (network == mNetworkData.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = network->mSecond.Remove(instanceID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (network->mSecond.IsEmpty()) {
        if (auto providerIt = mNetworkProviders.Find(networkID); providerIt == mNetworkProviders.end()) {
            if (auto err = ClearNetwork(networkID); !err.IsNone()) {
                return err;
            }
        }

        if (auto err = mNetworkData.Remove(networkID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::ClearNetwork(const String& networkID)
{
    LOG_DBG() << "Clear network: networkID=" << networkID;

    StaticString<cInterfaceLen> ifName;

    if (auto err = mNetIf->DeleteLink(ifName.Append(cBridgePrefix).Append(networkID)); !err.IsNone()) {
        return err;
    }

    if (auto itProvider = mNetworkProviders.Find(networkID);
        itProvider != mNetworkProviders.end() && !itProvider->mSecond.mVlanIfName.IsEmpty()) {
        if (auto err = mNetIf->DeleteLink(itProvider->mSecond.mVlanIfName); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = fs::RemoveAll(fs::JoinPath(mCNINetworkCacheDir, networkID)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareCNIConfig(const String& instanceID, const String& networkID,
    const InstanceNetworkParameters& network, cni::NetworkConfigList& netConfigList, cni::RuntimeConf& rtConfig,
    Array<StaticString<cHostNameLen>>& hosts) const
{
    LOG_DBG() << "Prepare CNI config: instanceID=" << instanceID << ", networkID=" << networkID;

    if (auto err = PrepareHosts(instanceID, networkID, network, hosts); !err.IsNone()) {
        return err;
    }

    netConfigList.mName    = networkID;
    netConfigList.mVersion = cni::cVersion;

    if (auto err = PrepareNetworkConfigList(instanceID, networkID, network, netConfigList); !err.IsNone()) {
        return err;
    }

    if (auto err = PrepareRuntimeConfig(instanceID, rtConfig, hosts); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareHosts(const String& instanceID, const String& networkID,
    const InstanceNetworkParameters& network, Array<StaticString<cHostNameLen>>& hosts) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Prepare hosts: networkID=" << networkID;

    auto networkData = mNetworkData.Find(networkID);
    if (networkData == mNetworkData.end()) {
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

    if (!network.mInstanceIdent.mServiceID.IsEmpty() && !network.mInstanceIdent.mSubjectID.IsEmpty()) {
        StaticString<cHostNameLen> host;

        host.Format("%d.%s.%s", network.mInstanceIdent.mInstance, network.mInstanceIdent.mSubjectID.CStr(),
            network.mInstanceIdent.mServiceID.CStr());

        if (auto err = PushHostWithDomain(host, networkID, hosts); !err.IsNone()) {
            return err;
        }

        if (network.mInstanceIdent.mInstance == 0) {
            host.Format("%s.%s", network.mInstanceIdent.mSubjectID.CStr(), network.mInstanceIdent.mServiceID.CStr());

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
            if (instance.mSecond.mHost.Find(host) != instance.mSecond.mHost.end()) {
                return ErrorEnum::eAlreadyExist;
            }
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateResolvConfFile(
    const String& networkID, const InstanceNetworkParameters& network, const Array<StaticString<cIPLen>>& dns) const
{
    LOG_DBG() << "Create resolv.conf file: networkID=" << networkID;

    if (network.mResolvConfFilePath.IsEmpty()) {
        return ErrorEnum::eNone;
    }

    StaticArray<StaticString<cIPLen>, cMaxNumDNSServers> mainServers {dns};

    if (mainServers.IsEmpty()) {
        mainServers.PushBack("8.8.8.8");
    }

    return WriteResolvConfFile(network.mResolvConfFilePath, mainServers, network);
}

Error NetworkManager::WriteResolvConfFile(const String& filePath, const Array<StaticString<cIPLen>>& mainServers,
    const InstanceNetworkParameters& network) const
{
    LOG_DBG() << "Write resolv.conf file: filePath=" << filePath;

    auto fd = open(filePath.CStr(), O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        return Error(errno);
    }

    auto closeFile = DeferRelease(&fd, [](const int* fd) { close(*fd); });

    auto writeNameServers = [&fd](Array<StaticString<cIPLen>> servers) -> Error {
        for (const auto& server : servers) {
            StaticString<cResolvConfLineLen> line;

            if (auto err = line.Format("nameserver\t%s\n", server.CStr()); !err.IsNone()) {
                return err;
            }

            const auto buff = Array<uint8_t>(reinterpret_cast<const uint8_t*>(line.Get()), line.Size());

            size_t pos = 0;

            while (pos < buff.Size()) {
                auto chunkSize = write(fd, buff.Get() + pos, buff.Size() - pos);
                if (chunkSize < 0) {
                    return Error(errno);
                }

                pos += chunkSize;
            }
        }

        return ErrorEnum::eNone;
    };

    if (auto err = writeNameServers(mainServers); !err.IsNone()) {
        return err;
    }

    return writeNameServers(network.mDNSSevers);
}

Error NetworkManager::CreateHostsFile(
    const String& networkID, const String& instanceIP, const InstanceNetworkParameters& network) const
{
    LOG_DBG() << "Create hosts file: networkID=" << networkID;

    if (network.mHostsFilePath.IsEmpty()) {
        return ErrorEnum::eNone;
    }

    StaticArray<SharedPtr<Host>, cMaxNumHosts * 3> hosts;

    auto localhost = MakeShared<Host>(&mHostAllocator, String("127.0.0.1"), String("localhost"));

    if (auto err = hosts.PushBack(localhost); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto localhost6 = MakeShared<Host>(&mHostAllocator, String("::1"), String("localhost ip6-localhost ip6-loopback"));

    if (auto err = hosts.PushBack(localhost6); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<cHostNameLen> ownHosts {networkID};

    if (!network.mHostname.IsEmpty()) {
        ownHosts.Append(" ").Append(network.mHostname);
    }

    auto instanceHost = MakeShared<Host>(&mHostAllocator, instanceIP, ownHosts);

    if (auto err = hosts.PushBack(instanceHost); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return WriteHostsFile(network.mHostsFilePath, hosts, network);
}

Error NetworkManager::WriteHostsFile(
    const String& filePath, const Array<SharedPtr<Host>>& hosts, const InstanceNetworkParameters& network) const
{
    LOG_DBG() << "Write hosts file: filePath=" << filePath;

    auto fd = open(filePath.CStr(), O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        return Error(errno);
    }

    auto closeFile = DeferRelease(&fd, [](const int* fd) { close(*fd); });

    if (auto err = WriteHosts(hosts, fd); !err.IsNone()) {
        return err;
    }

    return WriteHosts(network.mHosts, fd);
}

Error NetworkManager::WriteHost(const Host& host, int fd) const
{
    StaticString<cHostNameLen> line;

    if (auto err = line.Format("%s\t%s\n", host.mIP.CStr(), host.mHostname.CStr()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    const auto buff = Array<uint8_t>(reinterpret_cast<const uint8_t*>(line.Get()), line.Size());

    size_t pos = 0;
    while (pos < buff.Size()) {
        auto chunkSize = write(fd, buff.Get() + pos, buff.Size() - pos);
        if (chunkSize < 0) {
            return Error(errno);
        }

        pos += chunkSize;
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::WriteHosts(Array<SharedPtr<Host>> hosts, int fd) const
{
    for (const auto& host : hosts) {
        if (auto err = WriteHost(*host, fd); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
};

Error NetworkManager::WriteHosts(Array<Host> hosts, int fd) const
{
    for (const auto& host : hosts) {
        if (auto err = WriteHost(host, fd); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
};

Error NetworkManager::PrepareRuntimeConfig(
    const String& instanceID, cni::RuntimeConf& rt, const Array<StaticString<cHostNameLen>>& hosts) const
{
    LOG_DBG() << "Prepare runtime config: instanceID=" << instanceID;

    rt.mContainerID = instanceID;

    Error err;

    Tie(rt.mNetNS, err) = GetNetnsPath(instanceID);
    if (!err.IsNone()) {
        return err;
    }

    rt.mIfName = cInstanceInterfaceName;

    rt.mArgs.PushBack({"IgnoreUnknown", "1"});
    rt.mArgs.PushBack({"K8S_POD_NAME", instanceID});

    if (!hosts.IsEmpty()) {
        rt.mCapabilityArgs.mHost = hosts;
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareNetworkConfigList(const String& instanceID, const String& networkID,
    const InstanceNetworkParameters& network, cni::NetworkConfigList& net) const
{
    LOG_DBG() << "Prepare network config list: instanceID=" << instanceID << ", networkID=" << networkID;

    if (auto err = CreateBridgePluginConfig(networkID, network, net.mBridge); !err.IsNone()) {
        return err;
    }

    if (auto err = CreateFirewallPluginConfig(instanceID, network, net.mFirewall); !err.IsNone()) {
        return err;
    }

    if (auto err = CreateBandwidthPluginConfig(network, net.mBandwidth); !err.IsNone()) {
        return err;
    }

    if (auto err = CreateDNSPluginConfig(networkID, network, net.mDNS); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateBridgePluginConfig(
    const String& networkID, const InstanceNetworkParameters& network, cni::BridgePluginConf& config) const
{
    LOG_DBG() << "Create bridge plugin config";

    config.mType = "bridge";
    config.mBridge.Append(cBridgePrefix).Append(networkID);
    config.mIsGateway   = true;
    config.mIPMasq      = true;
    config.mHairpinMode = true;

    config.mIPAM.mType              = "host-local";
    config.mIPAM.mDataDir           = mCNINetworkCacheDir;
    config.mIPAM.mRange.mRangeStart = network.mNetworkParameters.mIP;
    config.mIPAM.mRange.mRangeEnd   = network.mNetworkParameters.mIP;
    config.mIPAM.mRange.mSubnet     = network.mNetworkParameters.mSubnet;

    if (auto it = mNetworkProviders.Find(networkID); it != mNetworkProviders.end()) {
        config.mIPAM.mRange.mGateway = it->mSecond.mIP;
    }

    if (auto err = config.mIPAM.mRouters.Resize(1); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    config.mIPAM.mRouters.Back().mDst = "0.0.0.0/0";

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateFirewallPluginConfig(
    const String& instanceID, const InstanceNetworkParameters& network, cni::FirewallPluginConf& config) const
{
    LOG_DBG() << "Create firewall plugin config";

    config.mType = "aos-firewall";
    config.mIptablesAdminChainName.Append(cAdminChainPrefix).Append(instanceID);
    config.mUUID                   = instanceID;
    config.mAllowPublicConnections = true;

    StaticArray<StaticString<cPortLen>, cMaxExposedPort> portConfig;

    for (const auto& port : network.mExposedPorts) {
        if (auto err = port.Split(portConfig, '/'); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (portConfig.IsEmpty()) {
            return ErrorEnum::eInvalidArgument;
        }

        if (auto err
            = config.mInputAccess.PushBack({portConfig[0], portConfig.Size() > 1 ? portConfig[1] : String("tcp")});
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (const auto& rule : network.mNetworkParameters.mFirewallRules) {
        if (auto err = config.mOutputAccess.PushBack({rule.mDstIP, rule.mDstPort, rule.mProto, rule.mSrcIP});
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateBandwidthPluginConfig(
    const InstanceNetworkParameters& network, cni::BandwidthNetConf& config) const
{
    if (network.mIngressKbit == 0 && network.mEgressKbit == 0) {
        return ErrorEnum::eNone;
    }

    LOG_DBG() << "Create bandwidth plugin config";

    config.mType = "bandwidth";

    if (network.mIngressKbit > 0) {
        config.mIngressRate  = network.mIngressKbit * 1000;
        config.mIngressBurst = cBurstLen;
    }

    if (network.mEgressKbit > 0) {
        config.mEgressRate  = network.mEgressKbit * 1000;
        config.mEgressBurst = cBurstLen;
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateDNSPluginConfig(
    const String& networkID, const InstanceNetworkParameters& network, cni::DNSPluginConf& config) const
{
    LOG_DBG() << "Create DNS plugin config";

    config.mType        = "dnsname";
    config.mMultiDomain = true;
    config.mDomainName  = networkID;

    for (const auto& dnsServer : network.mDNSSevers) {
        if (auto err = config.mRemoteServers.PushBack(dnsServer); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    config.mCapabilities.mAliases = true;

    return ErrorEnum::eNone;
}

Error NetworkManager::RemoveNetworks(const Array<aos::NetworkParameters>& networks)
{
    Error err;

    UniqueLock lock {mMutex};

    for (auto it = mNetworkProviders.begin(); it != mNetworkProviders.end();) {
        auto itNetwork = networks.FindIf([&](const auto& network) { return it->mFirst == network.mNetworkID; });
        if (itNetwork == networks.end()) {
            lock.Unlock();

            if (auto errRemoveNetwork = RemoveNetwork(it->mFirst);
                !errRemoveNetwork.IsNone() && !errRemoveNetwork.Is(ErrorEnum::eNotFound) && err.IsNone()) {
                err = errRemoveNetwork;
            }

            lock.Lock();

            if (auto errClearNetwork = ClearNetwork(it->mFirst); !errClearNetwork.IsNone() && err.IsNone()) {
                err = errClearNetwork;
            }

            it = mNetworkProviders.Erase(it);

            if (auto errRemoveStorage = mStorage->RemoveNetworkInfo(it->mFirst);
                !errRemoveStorage.IsNone() && err.IsNone()) {
                err = errRemoveStorage;
            }
        } else {
            ++it;
        }
    }

    return err;
}

Error NetworkManager::RemoveNetwork(const String& networkID)
{
    NetworkCache::Iterator provider;

    {
        LOG_DBG() << "Remove network: networkID=" << networkID;

        UniqueLock lock {mMutex};

        provider = mNetworkData.Find(networkID);
        if (provider == mNetworkData.end()) {
            return ErrorEnum::eNotFound;
        }
    }

    Error err;

    for (const auto& instance : provider->mSecond) {
        if (auto errRemoveInstance = RemoveInstanceFromNetwork(instance.mFirst, networkID);
            !errRemoveInstance.IsNone() && err.IsNone()) {
            err = errRemoveInstance;
        }
    }

    return err;
}

Error NetworkManager::CreateNetwork(const NetworkInfo& network)
{
    LOG_DBG() << "Create network: networkID=" << network.mNetworkID << ", subnet=" << network.mSubnet
              << ", ip=" << network.mIP << ", vlanID=" << network.mVlanID << ", vlanIfName=" << network.mVlanIfName;

    StaticString<cInterfaceLen> bridgeName;

    if (auto err = mNetIfFactory->CreateBridge(
            bridgeName.Append(cBridgePrefix).Append(network.mNetworkID), network.mIP, network.mSubnet);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNetIfFactory->CreateVlan(network.mVlanIfName, network.mVlanID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNetIf->SetMasterLink(network.mVlanIfName, bridgeName); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Network created: networkID=" << network.mNetworkID << ", subnet=" << network.mSubnet
              << ", ip=" << network.mIP << ", vlanID=" << network.mVlanID << ", vlanIfName=" << network.mVlanIfName;

    return ErrorEnum::eNone;
}

Error NetworkManager::GenerateVlanIfName(String& vlanIfName)
{
    vlanIfName.Append(cVlanIfPrefix);

    String randomString
        = String(vlanIfName.Get() + strlen(cVlanIfPrefix), vlanIfName.MaxSize() - strlen(cVlanIfPrefix));

    for (auto i = 0; i < cCountRetriesVlanIfNameGen; ++i) {
        if (auto err = crypto::GenerateRandomString<4>(randomString, *mRandom); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        vlanIfName.Resize(vlanIfName.Size() + randomString.Size());

        auto it
            = mNetworkProviders.FindIf([&](const auto& network) { return network.mSecond.mVlanIfName == vlanIfName; });
        if (it == mNetworkProviders.end()) {
            return ErrorEnum::eNone;
        }

        vlanIfName.Resize(vlanIfName.Size() - randomString.Size());
    }

    return ErrorEnum::eNotFound;
}

} // namespace aos::sm::networkmanager
