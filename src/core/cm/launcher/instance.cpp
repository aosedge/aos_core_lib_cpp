/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>
#include <core/common/tools/memory.hpp>

#include "instance.hpp"

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Instance implementation
 **********************************************************************************************************************/

Instance::Instance(
    const InstanceInfo& info, StorageItf& storage, ImageInfoProvider& imageInfoProvider, Allocator& allocator)
    : mInfo(info)
    , mStorage(storage)
    , mImageInfoProvider(imageInfoProvider)
    , mAllocator(allocator)
{
    static_cast<InstanceIdent&>(mStatus) = info.mInstanceIdent;
    mStatus.mError                       = ErrorEnum::eNone;
    mStatus.mNodeID                      = info.mNodeID;
    mStatus.mRuntimeID                   = info.mRuntimeID;
}

bool Instance::IsImageValid()
{
    // If manifest digest is empty, instance is not running yet, so treat it as valid.
    if (mInfo.mManifestDigest.IsEmpty()) {
        return true;
    }

    auto imageIndex = MakeUnique<oci::ImageIndex>(&mAllocator);

    auto err = mImageInfoProvider.GetImageIndex(mInfo.mInstanceIdent.mItemID, mInfo.mVersion, *imageIndex);
    if (!err.IsNone()) {
        return false;
    }

    auto imageDescriptor = imageIndex->mManifests.FindIf(
        [&](const oci::IndexContentDescriptor& descriptor) { return descriptor.mDigest == mInfo.mManifestDigest; });

    return imageDescriptor != nullptr;
}

Error Instance::UpdateStatus(const InstanceStatus& status)
{
    mStatus = status;

    mInfo.mNodeID = status.mNodeID;

    if (auto err = mStorage.UpdateInstance(mInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Instance::SetError(const Error& err)
{
    mInfo.mPrevNodeID = mInfo.mNodeID;
    mInfo.mNodeID     = "";

    mStatus.mError = err;
    mStatus.mState = aos::InstanceStateEnum::eFailed;

    if (auto updateErr = mStorage.UpdateInstance(mInfo); !updateErr.IsNone()) {
        LOG_ERR() << "Can't set instance error status" << Log::Field(err);
    }
}

void Instance::UpdateMonitoringData(const MonitoringData& monitoringData)
{
    mMonitoringData = monitoringData;
}

bool Instance::IsPlatformOk(const PlatformInfo& platformInfo)
{
    assert(mImageConfig);

    // Required params: OS, architecture
    if (platformInfo.mOSInfo.mOS != mImageConfig->mOS
        || platformInfo.mArchInfo.mArchitecture != mImageConfig->mArchitecture) {
        return false;
    }

    // Optional params: architecture variant, OS version, OS features
    if (!mImageConfig->mVariant.IsEmpty()) {
        if (platformInfo.mArchInfo.mVariant != mImageConfig->mVariant) {
            return false;
        }
    }

    if (!mImageConfig->mOSVersion.IsEmpty()) {
        if (platformInfo.mOSInfo.mVersion != mImageConfig->mOSVersion) {
            return false;
        }
    }

    if (!mImageConfig->mOSFeatures.IsEmpty()) {
        bool allFeaturesExist = true;

        for (const auto& imageFeature : mImageConfig->mOSFeatures) {
            bool exists = platformInfo.mOSInfo.mFeatures.Contains(imageFeature);
            if (!exists) {
                allFeaturesExist = false;
                break;
            }
        }

        if (!allFeaturesExist) {
            return false;
        }
    }

    return true;
}

bool Instance::AreNodeLabelsOk(const LabelsArray& nodeLabels)
{
    for (const auto& label : mInfo.mLabels) {
        if (!nodeLabels.Contains(label)) {
            return false;
        }
    }

    return true;
}

Error Instance::SetActive(const String& nodeID, const String& runtimeID)
{
    mInfo.mPrevNodeID = mInfo.mNodeID;
    mInfo.mNodeID     = nodeID;
    mInfo.mRuntimeID  = runtimeID;
    mInfo.mState      = InstanceStateEnum::eActive;

    mStatus.mPreinstalled   = false;
    mStatus.mNodeID         = nodeID;
    mStatus.mRuntimeID      = runtimeID;
    mStatus.mManifestDigest = mInfo.mManifestDigest;
    mStatus.mStateChecksum.Clear();
    mStatus.mEnvVarsStatuses.Clear();
    mStatus.mState = aos::InstanceStateEnum::eActivating;
    mStatus.mError = ErrorEnum::eNone;

    if (auto err = mStorage.UpdateInstance(mInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * ComponentInstance implementation
 **********************************************************************************************************************/

ComponentInstance::ComponentInstance(
    const InstanceInfo& info, StorageItf& storage, ImageInfoProvider& imageInfoProvider, Allocator& allocator)
    : Instance(info, storage, imageInfoProvider, allocator)
{
}

Error ComponentInstance::Init()
{
    return ErrorEnum::eNone;
}

Error ComponentInstance::LoadConfigs(const oci::IndexContentDescriptor& imageDescriptor)
{
    mImageConfig = MakeUnique<oci::ImageConfig>(&mAllocator);

    auto releaseConfig = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { ResetConfigs(); });
    if (auto err = mImageInfoProvider.GetImageConfig(imageDescriptor, *mImageConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "get image config failed"));
    }

    releaseConfig.Release();

    mInfo.mManifestDigest = imageDescriptor.mDigest;

    return ErrorEnum::eNone;
}

void ComponentInstance::ResetConfigs()
{
    mImageConfig.Reset();
}

Error ComponentInstance::Remove()
{
    LOG_DBG() << "Remove instance" << Log::Field("instanceID", mInfo.mInstanceIdent);

    if (auto err = mStorage.RemoveInstance(mInfo.mInstanceIdent); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ComponentInstance::Cache(bool disable)
{
    LOG_DBG() << "Cache instance" << Log::Field("instanceID", mInfo.mInstanceIdent) << Log::Field("disable", disable);

    mInfo.mState  = disable ? InstanceStateEnum::eDisabled : InstanceStateEnum::eCached;
    mInfo.mNodeID = "";

    if (auto err = mStorage.UpdateInstance(mInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool ComponentInstance::IsAvailableCpuOk(size_t availableCPU, const NodeConfig& nodeConfig, bool useMonitoringData)
{
    (void)availableCPU;
    (void)nodeConfig;
    (void)useMonitoringData;

    return true;
}

bool ComponentInstance::IsAvailableRamOk(size_t availableRAM, const NodeConfig& nodeConfig, bool useMonitoringData)
{
    (void)availableRAM;
    (void)nodeConfig;
    (void)useMonitoringData;

    return true;
}

bool ComponentInstance::IsRuntimeTypeOk(const StaticString<cRuntimeTypeLen>& runtimeType)
{
    (void)runtimeType;

    return true;
}

bool ComponentInstance::AreNodeResourcesOk(const ResourceInfoArray& nodeResources)
{
    (void)nodeResources;

    return true;
}

oci::BalancingPolicyEnum ComponentInstance::GetBalancingPolicy()
{
    return oci::BalancingPolicyEnum::eBalancingDisabled;
}

Error ComponentInstance::Schedule(NodeItf& node, const String& runtimeID, aos::InstanceInfo& info)
{
    auto releaseConfig = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { mImageConfig = nullptr; });

    static_cast<InstanceIdent&>(info) = mInfo.mInstanceIdent;
    info.mVersion                     = mInfo.mVersion;
    info.mManifestDigest              = mInfo.mManifestDigest;
    info.mRuntimeID                   = runtimeID;
    info.mOwnerID                     = mInfo.mOwnerID;
    info.mSubjectType                 = mInfo.mSubjectType;
    info.mUID                         = mInfo.mUID;
    info.mGID                         = mInfo.mGID;
    info.mPriority                    = mInfo.mPriority;

    info.mStoragePath = "";
    info.mStatePath   = "";
    info.mEnvVars.Clear();
    info.mNetworkParameters.Reset();
    info.mMonitoringParams.Reset();

    if (auto err = node.ScheduleInstance(info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetActive(node.GetConfig().mNodeID, runtimeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * ServiceInstance implementation
 **********************************************************************************************************************/

ServiceInstance::ServiceInstance(const InstanceInfo& info, UIDPool& uidPool, GIDPool& gidPool, StorageItf& storage,
    StorageState& storageState, ImageInfoProvider& imageInfoProvider, NetworkManager& networkManager,
    Allocator& allocator)
    : Instance(info, storage, imageInfoProvider, allocator)
    , mUIDPool(uidPool)
    , mGIDPool(gidPool)
    , mStorageState(storageState)
    , mNetworkManager(networkManager)
{
}

Error ServiceInstance::Init()
{
    if (mInfo.mUID != 0) {
        if (auto err = mUIDPool.TryAcquire(mInfo.mUID); !err.IsNone()) {
            LOG_WRN() << "Can't add UID to pool" << Log::Field(err);
        }
    } else {
        Error err;

        Tie(mInfo.mUID, err) = mUIDPool.Acquire();
        if (!err.IsNone()) {
            LOG_WRN() << "Can't add UID to pool" << Log::Field(err);
        }
    }

    gid_t gid;
    Error gidErr;

    Tie(gid, gidErr) = mGIDPool.GetGID(mInfo.mInstanceIdent.mItemID, mInfo.mGID);
    if (!gidErr.IsNone()) {
        return AOS_ERROR_WRAP(gidErr);
    }

    mInfo.mGID = gid;

    return ErrorEnum::eNone;
}

Error ServiceInstance::LoadConfigs(const oci::IndexContentDescriptor& imageDescriptor)
{
    mServiceConfig = MakeUnique<oci::ServiceConfig>(&mAllocator);
    mImageConfig   = MakeUnique<oci::ImageConfig>(&mAllocator);

    auto releaseConfigs = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { ResetConfigs(); });
    if (auto err = mImageInfoProvider.GetServiceConfig(imageDescriptor, *mServiceConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "get service config failed"));
    }

    if (auto err = mImageInfoProvider.GetImageConfig(imageDescriptor, *mImageConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "get image config failed"));
    }

    releaseConfigs.Release();

    mInfo.mManifestDigest = imageDescriptor.mDigest;

    return ErrorEnum::eNone;
}

void ServiceInstance::ResetConfigs()
{
    mServiceConfig.Reset();
    mImageConfig.Reset();
}

Error ServiceInstance::Remove()
{
    LOG_DBG() << "Remove instance" << Log::Field("instanceID", mInfo.mInstanceIdent);

    if (auto err = mStorageState.Remove(mInfo.mInstanceIdent); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorage.RemoveInstance(mInfo.mInstanceIdent); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mUIDPool.Release(mInfo.mUID); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mGIDPool.Release(mInfo.mInstanceIdent.mItemID); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ServiceInstance::Cache(bool disable)
{
    LOG_DBG() << "Cache instance" << Log::Field("instanceID", mInfo.mInstanceIdent) << Log::Field("disable", disable);

    mInfo.mState  = disable ? InstanceStateEnum::eDisabled : InstanceStateEnum::eCached;
    mInfo.mNodeID = "";

    if (auto err = mStorage.UpdateInstance(mInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorageState.Cleanup(mInfo.mInstanceIdent); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool ServiceInstance::IsAvailableCpuOk(size_t availableCPU, const NodeConfig& nodeConfig, bool useMonitoringData)
{
    assert(mServiceConfig);

    auto requestedCPU = GetRequestedCPU(nodeConfig, useMonitoringData);

    bool ok = availableCPU >= requestedCPU;

    LOG_DBG() << "Available CPU " << (ok ? "enough" : "not enough") << Log::Field("nodeID", nodeConfig.mNodeID)
              << Log::Field("availableCPU", availableCPU) << Log::Field("requestedCPU", requestedCPU);

    return ok;
}

bool ServiceInstance::IsAvailableRamOk(size_t availableRAM, const NodeConfig& nodeConfig, bool useMonitoringData)
{
    assert(mServiceConfig);

    auto requestedRAM = GetRequestedRAM(nodeConfig, useMonitoringData);

    bool ok = availableRAM >= requestedRAM;

    LOG_DBG() << "Available RAM " << (ok ? "enough" : "not enough") << Log::Field("nodeID", nodeConfig.mNodeID)
              << Log::Field("availableRAM", availableRAM) << Log::Field("requestedRAM", requestedRAM);

    return ok;
}

bool ServiceInstance::IsRuntimeTypeOk(const StaticString<cRuntimeTypeLen>& runtimeType)
{
    assert(mServiceConfig);

    return mServiceConfig->mRuntimes.Contains(runtimeType);
}

bool ServiceInstance::AreNodeResourcesOk(const ResourceInfoArray& nodeResources)
{
    assert(mServiceConfig);

    for (const auto& resource : mServiceConfig->mResources) {
        auto matchResource
            = [&resource](const ResourceInfo& info) { return info.mName == resource && info.mSharedCount > 0; };

        if (!nodeResources.ContainsIf(matchResource)) {
            return false;
        }
    }

    return true;
}

oci::BalancingPolicyEnum ServiceInstance::GetBalancingPolicy()
{
    assert(mServiceConfig);

    return mServiceConfig->mBalancingPolicy;
}

Error ServiceInstance::Schedule(NodeItf& node, const String& runtimeID, aos::InstanceInfo& info)
{
    assert(mServiceConfig);

    auto releaseConfigs = DeferRelease(reinterpret_cast<int*>(1), [&](int*) {
        mServiceConfig.Reset();
        mImageConfig.Reset();
    });

    static_cast<InstanceIdent&>(info) = mInfo.mInstanceIdent;
    info.mVersion                     = mInfo.mVersion;
    info.mManifestDigest              = mInfo.mManifestDigest;
    info.mRuntimeID                   = runtimeID;
    info.mOwnerID                     = mInfo.mOwnerID;
    info.mSubjectType                 = mInfo.mSubjectType;
    info.mUID                         = mInfo.mUID;
    info.mGID                         = mInfo.mGID;
    info.mPriority                    = mInfo.mPriority;

    if (auto err = SetupStateStorage(node.GetConfig(), info.mStoragePath, info.mStatePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    info.mEnvVars.Clear();

    if (auto err = SetupNetworkServiceData(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    info.mMonitoringParams.EmplaceValue();
    if (mServiceConfig->mAlertRules.HasValue()) {
        info.mMonitoringParams.GetValue().mAlertRules = mServiceConfig->mAlertRules.GetValue();
    }

    if (auto err = ReserveRuntimeResources(node); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = node.ScheduleInstance(info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetActive(node.GetConfig().mNodeID, runtimeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

size_t ServiceInstance::GetRequestedCPU(const NodeConfig& nodeConfig, bool useMonitoringData)
{
    assert(mServiceConfig);

    if (mServiceConfig->mSkipResourceLimits) {
        return 0;
    }

    size_t requestedCPU = 0;
    auto   quota        = mServiceConfig->mQuotas.mCPUDMIPSLimit;

    if (mServiceConfig->mRequestedResources.HasValue() && mServiceConfig->mRequestedResources->mCPU.HasValue()) {
        requestedCPU = ClampResource(*mServiceConfig->mRequestedResources->mCPU, quota);
    } else {
        requestedCPU = GetReqCPUFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    if (useMonitoringData) {
        if (mMonitoringData.mCPU > requestedCPU) {
            return mMonitoringData.mCPU;
        }
    }

    return requestedCPU;
}

size_t ServiceInstance::GetRequestedRAM(const NodeConfig& nodeConfig, bool useMonitoringData)
{
    assert(mServiceConfig);

    if (mServiceConfig->mSkipResourceLimits) {
        return 0;
    }

    size_t requestedRAM = 0;
    auto   quota        = mServiceConfig->mQuotas.mRAMLimit;

    if (mServiceConfig->mRequestedResources.HasValue() && mServiceConfig->mRequestedResources->mRAM.HasValue()) {
        requestedRAM = ClampResource(*mServiceConfig->mRequestedResources->mRAM, quota);
    } else {
        requestedRAM = GetReqRAMFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    if (useMonitoringData) {
        if (mMonitoringData.mRAM > requestedRAM) {
            return mMonitoringData.mRAM;
        }
    }

    return requestedRAM;
}

size_t ServiceInstance::GetReqStateSize(const NodeConfig& nodeConfig)
{
    size_t requestedState = 0;
    auto   quota          = mServiceConfig->mQuotas.mStateLimit;

    if (mServiceConfig->mRequestedResources.HasValue() && mServiceConfig->mRequestedResources->mState.HasValue()) {
        requestedState = ClampResource(*mServiceConfig->mRequestedResources->mState, quota);
    } else {
        requestedState = GetReqStateFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    return requestedState;
}

size_t ServiceInstance::GetReqStorageSize(const NodeConfig& nodeConfig)
{
    size_t requestedStorage = 0;
    auto   quota            = mServiceConfig->mQuotas.mStorageLimit;

    if (mServiceConfig->mRequestedResources.HasValue() && mServiceConfig->mRequestedResources->mStorage.HasValue()) {
        requestedStorage = ClampResource(*mServiceConfig->mRequestedResources->mStorage, quota);
    } else {
        requestedStorage = GetReqStorageFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    return requestedStorage;
}

size_t ServiceInstance::ClampResource(size_t value, const Optional<size_t>& quota)
{
    if (quota.HasValue() && value > quota.GetValue()) {
        return quota.GetValue();
    }

    return value;
}

size_t ServiceInstance::GetReqCPUFromNodeConfig(
    const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios)
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mCPU.HasValue()) {
        ratio = nodeRatios->mCPU.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<size_t>(*quota * ratio + 0.5);
    }

    return 0;
}

size_t ServiceInstance::GetReqRAMFromNodeConfig(
    const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios)
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mRAM.HasValue()) {
        ratio = nodeRatios->mRAM.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<size_t>(*quota * ratio + 0.5);
    }

    return 0;
}

size_t ServiceInstance::GetReqStateFromNodeConfig(
    const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios)
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mState.HasValue()) {
        ratio = nodeRatios->mState.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<size_t>(*quota * ratio + 0.5);
    }

    return 0;
}

size_t ServiceInstance::GetReqStorageFromNodeConfig(
    const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios)
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mStorage.HasValue()) {
        ratio = nodeRatios->mStorage.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<size_t>(*quota * ratio + 0.5);
    }

    return 0;
}

Error ServiceInstance::SetupNetworkServiceData()
{
    mNetworkServiceData.mExposedPorts       = mImageConfig->mConfig.mExposedPorts;
    mNetworkServiceData.mAllowedConnections = mServiceConfig->mAllowedConnections;

    if (mServiceConfig->mHostname.HasValue()) {
        mNetworkServiceData.mHosts.PushBack(mServiceConfig->mHostname.GetValue());
    }

    if (auto err = mNetworkManager.SetNetworkServiceData(mInfo.mInstanceIdent, mNetworkServiceData); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ServiceInstance::SetupStateStorage(const NodeConfig& nodeConfig, String& storagePath, String& statePath)
{
    auto reqState   = GetReqStateSize(nodeConfig);
    auto reqStorage = GetReqStorageSize(nodeConfig);

    storagestate::SetupParams params;

    params.mUID = mInfo.mUID;
    params.mGID = mInfo.mGID;

    if (mServiceConfig->mQuotas.mStorageLimit.HasValue()) {
        params.mStorageQuota = *mServiceConfig->mQuotas.mStorageLimit;
    }

    if (mServiceConfig->mQuotas.mStateLimit.HasValue()) {
        params.mStateQuota = *mServiceConfig->mQuotas.mStateLimit;
    }

    if (mServiceConfig->mSkipResourceLimits) {
        reqState   = 0;
        reqStorage = 0;
    }

    auto err
        = mStorageState.SetupStateStorage(mInfo.mInstanceIdent, params, reqStorage, reqState, storagePath, statePath);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ServiceInstance::ReserveRuntimeResources(NodeItf& node)
{
    auto requestedCPU = mServiceConfig->mSkipResourceLimits ? 0 : GetRequestedCPU(node.GetConfig(), false);
    auto requestedRAM = mServiceConfig->mSkipResourceLimits ? 0 : GetRequestedRAM(node.GetConfig(), false);
    Array<StaticString<cResourceNameLen>> requestedResources
        = mServiceConfig->mSkipResourceLimits ? Array<StaticString<cResourceNameLen>>() : mServiceConfig->mResources;

    auto reserveErr
        = node.ReserveResources(mInfo.mInstanceIdent, mInfo.mRuntimeID, requestedCPU, requestedRAM, requestedResources);
    if (!reserveErr.IsNone()) {
        return AOS_ERROR_WRAP(reserveErr);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
