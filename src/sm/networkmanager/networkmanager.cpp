/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/networkmanager.hpp"
#include "aos/common/tools/memory.hpp"

#include "log.hpp"

namespace aos::sm::networkmanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error NetworkManager::Init(StorageItf& storage, cni::CNIItf& cni, TrafficMonitorItf& netMonitor,
    NamespaceManagerItf& netns, NetworkInterfaceManagerItf& netIf, const String& workingDir)
{
    LOG_DBG() << "Initialize network manager";

    mStorage    = &storage;
    mCNI        = &cni;
    mNetMonitor = &netMonitor;
    mNetns      = &netns;
    mNetIf      = &netIf;

    auto cniDir = FS::JoinPath(workingDir, "cni");

    if (auto err = mCNI->Init(cniDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mCNINetworkCacheDir = FS::JoinPath(cniDir, "networks");

    return ErrorEnum::eNone;
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

    (void)networks;

    return ErrorEnum::eNone;
}

Error NetworkManager::AddInstanceToNetwork(
    const String& instanceID, const String& networkID, const NetworkParams& network)
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

    cni::NetworkConfigList                                netConfigList;
    cni::RuntimeConf                                      rtConfig;
    StaticArray<StaticString<cHostNameLen>, cMaxNumHosts> host;

    if (err = PrepareCNIConfig(instanceID, networkID, network, netConfigList, rtConfig, host); !err.IsNone()) {
        return err;
    }

    cni::Result result;

    Tie(result, err) = mCNI->AddNetworkList(netConfigList, rtConfig);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupCNI = DeferRelease(&instanceID, [this, &netConfigList, &rtConfig, &err](const String* instanceID) {
        if (!err.IsNone()) {
            if (auto errCleanCNI = mCNI->DeleteNetworkList(netConfigList, rtConfig); !errCleanCNI.IsNone()) {
                LOG_ERR() << "Failed to delete network list: instanceID=" << *instanceID << ", err=" << errCleanCNI;
            }
        }
    });

    if (err = mNetMonitor->StartInstanceMonitoring(
            instanceID, network.mNetworkParameters.mIP, network.mDownloadLimit, network.mUploadLimit);
        !err.IsNone()) {

        return AOS_ERROR_WRAP(err);
    }

    if (err = CreateHostsFile(networkID, network.mNetworkParameters.mIP, network); !err.IsNone()) {
        return err;
    }

    if (err = CreateResolvConfFile(networkID, network, result.mDNSServers); !err.IsNone()) {
        return err;
    }

    if (err = UpdateInstanceNetworkCache(instanceID, networkID, network.mNetworkParameters.mIP, host); !err.IsNone()) {
        return err;
    }

    LOG_DBG() << "Instance added to network: instanceID=" << instanceID << ", networkID=" << networkID;

    return ErrorEnum::eNone;
}

Error NetworkManager::RemoveInstanceFromNetwork(const String& instanceID, const String& networkID)
{
    LOG_DBG() << "Remove instance from network: instanceID=" << instanceID << ", networkID=" << networkID;

    if (auto err = IsInstanceInNetwork(instanceID, networkID); !err.IsNone()) {
        return err;
    }

    if (auto err = mNetMonitor->StopInstanceMonitoring(instanceID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    cni::NetworkConfigList netConfig;
    cni::RuntimeConf       rtConfig;

    netConfig.mName    = networkID;
    netConfig.mVersion = cni::cVersion;

    rtConfig.mContainerID = instanceID;

    Error err;

    Tie(rtConfig.mNetNS, err) = GetNetnsPath(instanceID);
    if (!err.IsNone()) {
        return err;
    }

    rtConfig.mIfName = cInstanceInterfaceName;

    if (err = mCNI->DeleteNetworkList(netConfig, rtConfig); !err.IsNone()) {
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

    auto network = mNetworkData.At(networkID);
    if (!network.mError.IsNone()) {
        return AOS_ERROR_WRAP(network.mError);
    }

    auto instance = network.mValue.At(instanceID);
    if (!instance.mError.IsNone()) {
        return AOS_ERROR_WRAP(instance.mError);
    }

    ip = instance.mValue.IPAddr;

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

    auto network = mNetworkData.At(networkID);
    if (!network.mError.IsNone()) {
        return AOS_ERROR_WRAP(network.mError);
    }

    if (auto instance = network.mValue.At(instanceID); !instance.mError.IsNone()) {
        return AOS_ERROR_WRAP(instance.mError);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::UpdateInstanceNetworkCache(const String& instanceID, const String& networkID,
    const String& instanceIP, const Array<StaticString<cHostNameLen>>& hosts)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Update instance network cache: instanceID=" << instanceID << ", networkID=" << networkID;

    auto instance = mNetworkData.At(networkID).mValue.At(instanceID);

    if (!instance.mError.IsNone()) {
        return AOS_ERROR_WRAP(instance.mError);
    }

    instance.mValue.IPAddr = instanceIP;
    instance.mValue.mHost  = hosts;

    return ErrorEnum::eNone;
}

Error NetworkManager::AddInstanceToCache(const String& instanceID, const String& networkID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Add instance to cache: instanceID=" << instanceID << ", networkID=" << networkID;

    auto network = mNetworkData.At(networkID);
    if (!network.mError.IsNone()) {
        if (!network.mError.Is(ErrorEnum::eNotFound)) {
            return AOS_ERROR_WRAP(network.mError);
        }

        mNetworkData.Set(networkID, InstanceCache());
    }

    if (auto err = mNetworkData.At(networkID).mValue.Set(instanceID, NetworkData()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::RemoveInstanceFromCache(const String& instanceID, const String& networkID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Remove instance from cache: instanceID=" << instanceID << ", networkID=" << networkID;

    auto network = mNetworkData.At(networkID);
    if (!network.mError.IsNone()) {
        return AOS_ERROR_WRAP(network.mError);
    }

    if (auto err = network.mValue.Remove(instanceID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (network.mValue.IsEmpty()) {
        if (auto err = ClearNetwork(networkID); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::ClearNetwork(const String& networkID)
{
    LOG_DBG() << "Clear network: networkID=" << networkID;

    StaticString<cInterfaceLen> ifName;

    if (auto err = mNetIf->RemoveInterface(ifName.Append(cBridgePrefix).Append(networkID)); !err.IsNone()) {
        return err;
    }

    if (auto err = FS::RemoveAll(FS::JoinPath(mCNINetworkCacheDir, networkID)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return AOS_ERROR_WRAP(mNetworkData.Remove(networkID));
}

Error NetworkManager::PrepareCNIConfig(const String& instanceID, const String& networkID, const NetworkParams& network,
    cni::NetworkConfigList& netConfigList, cni::RuntimeConf& rtConfig, Array<StaticString<cHostNameLen>>& hosts) const
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

Error NetworkManager::PrepareHosts(const String& instanceID, const String& networkID, const NetworkParams& network,
    Array<StaticString<cHostNameLen>>& hosts) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Prepare hosts: networkID=" << networkID;

    auto retNotworkData = mNetworkData.At(networkID);
    if (!retNotworkData.mError.IsNone()) {
        return AOS_ERROR_WRAP(retNotworkData.mError);
    }

    auto retInstanceData = retNotworkData.mValue.At(instanceID);
    if (!retInstanceData.mError.IsNone()) {
        return AOS_ERROR_WRAP(retInstanceData.mError);
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

    return IsHostnameExist(retNotworkData.mValue, hosts);
}

Error NetworkManager::PushHostWithDomain(
    const String& host, const String& networkID, Array<StaticString<cHostNameLen>>& hosts) const
{
    if (auto ret = hosts.Find(host); ret.mError.IsNone()) {
        return ErrorEnum::eAlreadyExist;
    }

    if (auto err = hosts.PushBack(host); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (host.Find('.').mError.IsNone()) {
        StaticString<cHostNameLen> withDomain;

        if (auto err = withDomain.Format("%s.%s", host.CStr(), networkID.CStr()); !err.IsNone()) {
            return err;
        }

        if (auto ret = hosts.Find(withDomain); ret.mError.IsNone()) {
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
            if (instance.mSecond.mHost.Find(host).mError.IsNone()) {
                return ErrorEnum::eAlreadyExist;
            }
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateResolvConfFile(
    const String& networkID, const NetworkParams& network, const Array<StaticString<cIPLen>>& dns) const
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

Error NetworkManager::WriteResolvConfFile(
    const String& filePath, const Array<StaticString<cIPLen>>& mainServers, const NetworkParams& network) const
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
    const String& networkID, const String& instanceIP, const NetworkParams& network) const
{
    LOG_DBG() << "Create hosts file: networkID=" << networkID;

    if (network.mHostsFilePath.IsEmpty()) {
        return ErrorEnum::eNone;
    }

    StaticArray<Host, cMaxNumHosts * 3> hosts;

    if (auto err = hosts.PushBack({"127.0.0.1", "localhost"}); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = hosts.PushBack({"::1", "localhost ip6-localhost ip6-loopback"}); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<cHostNameLen> ownHosts {networkID};

    if (!network.mHostname.IsEmpty()) {
        ownHosts.Append(" ").Append(network.mHostname);
    }

    if (auto err = hosts.PushBack({instanceIP, ownHosts}); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return WriteHostsFile(network.mHostsFilePath, hosts, network);
}

Error NetworkManager::WriteHostsFile(
    const String& filePath, const Array<Host>& hosts, const NetworkParams& network) const
{
    LOG_DBG() << "Write hosts file: filePath=" << filePath;

    auto fd = open(filePath.CStr(), O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        return Error(errno);
    }

    auto closeFile = DeferRelease(&fd, [](const int* fd) { close(*fd); });

    auto writeHosts = [&fd](Array<Host> hosts) -> Error {
        for (const auto& host : hosts) {
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
        }

        return ErrorEnum::eNone;
    };

    if (auto err = writeHosts(hosts); !err.IsNone()) {
        return err;
    }

    return writeHosts(network.mHosts);
}

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

Error NetworkManager::PrepareNetworkConfigList(
    const String& instanceID, const String& networkID, const NetworkParams& network, cni::NetworkConfigList& net) const
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
    const String& networkID, const NetworkParams& network, cni::BridgePluginConf& config) const
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

    config.mIPAM.mRouters.Resize(1);

    auto ret = config.mIPAM.mRouters.Back();
    if (!ret.mError.IsNone()) {
        return AOS_ERROR_WRAP(ret.mError);
    }

    ret.mValue.mDst = "0.0.0.0/0";

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateFirewallPluginConfig(
    const String& instanceID, const NetworkParams& network, cni::FirewallPluginConf& config) const
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

        if (auto err = config.mInputAccess.PushBack({portConfig[0], portConfig.Size() > 1 ? portConfig[1] : "tcp"});
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

Error NetworkManager::CreateBandwidthPluginConfig(const NetworkParams& network, cni::BandwidthNetConf& config) const
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
    const String& networkID, const NetworkParams& network, cni::DNSPluginConf& config) const
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

} // namespace aos::sm::networkmanager
