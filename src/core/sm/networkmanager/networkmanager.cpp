/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <unistd.h>

#include <core/common/tools/logger.hpp>
#include <core/common/tools/memory.hpp>

#include "networkmanager.hpp"

namespace aos::sm::networkmanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error NetworkManager::Init(StorageItf& storage, cni::CNIItf& cni, TrafficMonitorItf& netMonitor,
    NamespaceManagerItf& netns, InterfaceManagerItf& netIf, crypto::RandomItf& random,
    InterfaceFactoryItf& netIfFactory, const String& workingDir,
    aos::networkmanager::NetworkProviderItf& networkProvider, const String& nodeID)
{
    LOG_DBG() << "Init network manager";

    mStorage         = &storage;
    mCNI             = &cni;
    mNetMonitor      = &netMonitor;
    mNetns           = &netns;
    mNetIf           = &netIf;
    mRandom          = &random;
    mNetIfFactory    = &netIfFactory;
    mNetworkProvider = &networkProvider;
    mNodeID          = nodeID;

    auto cniDir      = fs::JoinPath(workingDir, "cni");
    auto cniCacheDir = fs::JoinPath(cniDir, "results");

    mCNINetworkCacheDir = fs::JoinPath(cniDir, "networks");

    if (auto err = mCNI->SetConfDir(cniCacheDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto instanceNetworkInfos
        = MakeUnique<StaticArray<InstanceNetworkInfo, cMaxNumInstances>>(&mInstanceNetworkInfosAllocator);

    if (auto err = mStorage->GetInstanceNetworksInfo(*instanceNetworkInfos); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& instanceNetworkInfo : *instanceNetworkInfos) {
        if (auto err = DeleteInstanceNetworkConfig(instanceNetworkInfo.mInstanceID, instanceNetworkInfo.mNetworkID);
            !err.IsNone()) {
            LOG_WRN() << "Failed to delete instance network config"
                      << Log::Field("instanceID", instanceNetworkInfo.mInstanceID)
                      << Log::Field("networkID", instanceNetworkInfo.mNetworkID) << Log::Field(err);
        }

        mInstanceNetworkInfos.Set(instanceNetworkInfo.mInstanceID, instanceNetworkInfo);
    }

    if (auto err = fs::RemoveAll(cniDir); !err.IsNone()) {
        LOG_ERR() << "Failed to remove cni directory: " << cniDir;
    }

    if (auto err = fs::MakeDirAll(cniCacheDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
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

    for (const auto& provider : mNetworkProviders) {
        if (auto err = ClearNetwork(provider.mSecond); !err.IsNone()) {
            LOG_ERR() << "Can't clear network" << Log::Field(err);
        }
    }
}

Error NetworkManager::Start()
{
    LOG_DBG() << "Start network manager";

    auto err = mNetMonitor->Start();

    return AOS_ERROR_WRAP(err);
}

Error NetworkManager::Stop()
{
    LOG_DBG() << "Stop network manager";

    auto err = mNetMonitor->Stop();

    return AOS_ERROR_WRAP(err);
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

Error NetworkManager::StartInstanceNetwork(
    const String& instanceID, const String& networkID, const InstanceNetworkRuntimeParams& runtimeParams)
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

    err = AddInstanceToNetwork(
        instanceID, networkID, cachedInfo->mNetworkConfig, cachedInfo->mAllocatedParams, runtimeParams);

    return err;
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
    const InstanceNetworkConfig& networkConfig, const aos::InstanceNetworkAllocation& networkParams,
    const InstanceNetworkRuntimeParams& runtimeParams)
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

    auto netConfigList = MakeUnique<cni::NetworkConfigList>(&mAllocator);
    auto rtConfig      = MakeUnique<cni::RuntimeConf>(&mAllocator);

    StaticArray<StaticString<cHostNameLen>, cMaxNumHosts> host;

    if (err = PrepareCNIConfig(instanceID, networkID, networkConfig, networkParams, *netConfigList, *rtConfig, host);
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
                LOG_ERR() << "Failed to delete network list" << Log::Field("instanceID", *instanceID)
                          << Log::Field(errCleanCNI);
            }
        }
    });

    if (err = mNetMonitor->StartInstanceMonitoring(
            instanceID, networkParams.mIP, networkConfig.mDownloadLimit, networkConfig.mUploadLimit);
        !err.IsNone()) {

        return AOS_ERROR_WRAP(err);
    }

    if (err = CreateHostsFile(networkID, networkParams.mIP, networkConfig, runtimeParams.mHostsFilePath);
        !err.IsNone()) {
        return err;
    }

    if (err = CreateResolvConfFile(networkID, runtimeParams.mResolvConfFilePath, networkParams, result->mDNSServers);
        !err.IsNone()) {
        return err;
    }

    if (err = UpdateInstanceNetworkCache(instanceID, networkID, host); !err.IsNone()) {
        return err;
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

    return ErrorEnum::eNone;
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

    if (!networkInfo.mBridgeIfName.IsEmpty()) {
        if (auto err = mNetIf->DeleteLink(networkInfo.mBridgeIfName); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (!networkInfo.mVlanIfName.IsEmpty()) {
        if (auto err = mNetIf->DeleteLink(networkInfo.mVlanIfName); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = fs::RemoveAll(fs::JoinPath(mCNINetworkCacheDir, networkInfo.mNetworkID)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareCNIConfig(const String& instanceID, const String& networkID,
    const InstanceNetworkConfig& network, const aos::InstanceNetworkAllocation& networkParams,
    cni::NetworkConfigList& netConfigList, cni::RuntimeConf& rtConfig, Array<StaticString<cHostNameLen>>& hosts) const
{
    LOG_DBG() << "Prepare CNI config" << Log::Field("instanceID", instanceID) << Log::Field("networkID", networkID);

    if (auto err = PrepareHosts(instanceID, networkID, network, hosts); !err.IsNone()) {
        return err;
    }

    netConfigList.mName    = networkID;
    netConfigList.mVersion = cni::cVersion;

    if (auto err = PrepareNetworkConfigList(instanceID, networkID, network, networkParams, netConfigList);
        !err.IsNone()) {
        return err;
    }

    if (auto err = PrepareRuntimeConfig(instanceID, rtConfig, hosts); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
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

Error NetworkManager::CreateResolvConfFile(const String& networkID, const String& resolvConfFilePath,
    const aos::InstanceNetworkAllocation& networkParams, const Array<StaticString<cIPLen>>& dns) const
{
    LOG_DBG() << "Create resolv.conf file" << Log::Field("networkID", networkID);

    if (resolvConfFilePath.IsEmpty()) {
        return ErrorEnum::eNone;
    }

    StaticArray<StaticString<cIPLen>, cMaxNumDNSServers> mainServers {dns};

    if (mainServers.IsEmpty()) {
        mainServers.PushBack("8.8.8.8");
    }

    return WriteResolvConfFile(resolvConfFilePath, mainServers, networkParams);
}

Error NetworkManager::WriteResolvConfFile(const String& filePath, const Array<StaticString<cIPLen>>& mainServers,
    const aos::InstanceNetworkAllocation& networkParams) const
{
    LOG_DBG() << "Write resolv.conf file" << Log::Field("filePath", filePath);

    auto fd = open(filePath.CStr(), O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        return Error(errno);
    }

    auto closeFile = DeferRelease(&fd, [](const int* fd) { close(*fd); });

    auto writeNameServers = [&fd](const Array<StaticString<cIPLen>>& servers) -> Error {
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

    return writeNameServers(networkParams.mDNSServers);
}

Error NetworkManager::CreateHostsFile(const String& networkID, const String& instanceIP,
    const InstanceNetworkConfig& network, const String& hostsFilePath) const
{
    LOG_DBG() << "Create hosts file" << Log::Field("networkID", networkID);

    if (hostsFilePath.IsEmpty()) {
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

    return WriteHostsFile(hostsFilePath, hosts, network.mHosts);
}

Error NetworkManager::WriteHostsFile(
    const String& filePath, const Array<SharedPtr<Host>>& hosts, const Array<Host>& additionalHosts) const
{
    LOG_DBG() << "Write hosts file" << Log::Field("filePath", filePath);

    auto fd = open(filePath.CStr(), O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        return Error(errno);
    }

    auto closeFile = DeferRelease(&fd, [](const int* fd) { close(*fd); });

    if (auto err = WriteHosts(hosts, fd); !err.IsNone()) {
        return err;
    }

    return WriteHosts(additionalHosts, fd);
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

Error NetworkManager::WriteHosts(const Array<SharedPtr<Host>>& hosts, int fd) const
{
    for (const auto& host : hosts) {
        if (auto err = WriteHost(*host, fd); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
};

Error NetworkManager::WriteHosts(const Array<Host>& hosts, int fd) const
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
    LOG_DBG() << "Prepare runtime config" << Log::Field("instanceID", instanceID);

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
    const InstanceNetworkConfig& network, const aos::InstanceNetworkAllocation& networkParams,
    cni::NetworkConfigList& net) const
{
    LOG_DBG() << "Prepare network config list" << Log::Field("instanceID", instanceID)
              << Log::Field("networkID", networkID);

    if (auto err = CreateBridgePluginConfig(networkID, networkParams, net.mBridge); !err.IsNone()) {
        return err;
    }

    if (auto err = CreateFirewallPluginConfig(instanceID, network, networkParams, net.mFirewall); !err.IsNone()) {
        return err;
    }

    if (auto err = CreateBandwidthPluginConfig(network, net.mBandwidth); !err.IsNone()) {
        return err;
    }

    if (auto err = CreateDNSPluginConfig(networkID, networkParams, net.mDNS); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateBridgePluginConfig(
    const String& networkID, const aos::InstanceNetworkAllocation& networkParams, cni::BridgePluginConf& config) const
{
    LOG_DBG() << "Create bridge plugin config";

    LockGuard lock {mMutex};

    auto it = mNetworkProviders.Find(networkID);
    if (it == mNetworkProviders.end()) {
        return ErrorEnum::eNotFound;
    }

    config.mType        = "bridge";
    config.mBridge      = it->mSecond.mBridgeIfName;
    config.mIsGateway   = true;
    config.mIPMasq      = true;
    config.mHairpinMode = true;

    config.mIPAM.mType              = "host-local";
    config.mIPAM.mDataDir           = mCNINetworkCacheDir;
    config.mIPAM.mRange.mRangeStart = networkParams.mIP;
    config.mIPAM.mRange.mRangeEnd   = networkParams.mIP;
    config.mIPAM.mRange.mSubnet     = networkParams.mSubnet;
    config.mIPAM.mRange.mGateway    = it->mSecond.mIP;

    if (auto err = config.mIPAM.mRouters.Resize(1); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    config.mIPAM.mRouters.Back().mDst = "0.0.0.0/0";

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateFirewallPluginConfig(const String& instanceID, const InstanceNetworkConfig& network,
    const aos::InstanceNetworkAllocation& networkParams, cni::FirewallPluginConf& config) const
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

    for (const auto& rule : networkParams.mFirewallRules) {
        if (auto err = config.mOutputAccess.PushBack({rule.mDstIP, rule.mDstPort, rule.mProto, rule.mSrcIP});
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::CreateBandwidthPluginConfig(
    const InstanceNetworkConfig& network, cni::BandwidthNetConf& config) const
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
    const String& networkID, const aos::InstanceNetworkAllocation& networkParams, cni::DNSPluginConf& config) const
{
    LOG_DBG() << "Create DNS plugin config";

    config.mType        = "dnsname";
    config.mMultiDomain = true;
    config.mDomainName  = networkID;

    for (const auto& dnsServer : networkParams.mDNSServers) {
        if (auto err = config.mRemoteServers.PushBack(dnsServer); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    config.mCapabilities.mAliases = true;

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

    if (err = mNetIfFactory->CreateVlan(network.mVlanIfName, network.mVlanID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto cleanupVlan = DeferRelease(&network, [this, &err](const NetworkInfo* network) {
        if (!err.IsNone()) {
            mNetIf->DeleteLink(network->mVlanIfName);
        }
    });

    if (err = mNetIf->SetMasterLink(network.mVlanIfName, network.mBridgeIfName); !err.IsNone()) {
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

} // namespace aos::sm::networkmanager
