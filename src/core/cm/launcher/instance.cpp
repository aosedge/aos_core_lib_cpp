/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>
#include <core/common/tools/memory.hpp>

#include "instance.hpp"

namespace aos::cm::launcher {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

// UID is shared by all versions of the same instance, so the version isn't part of the UID pool key.
InstanceIdent GetUIDKey(const InstanceIdent& instanceIdent)
{
    auto key = instanceIdent;

    key.mVersion.Clear();

    return key;
}

} // namespace

/***********************************************************************************************************************
 * Instance implementation
 **********************************************************************************************************************/

Instance::Instance(
    AllocatorItf& allocator, const InstanceInfo& info, StorageItf& storage, ImageInfoProvider& imageInfoProvider)
    : mInfo(info)
    , mStorage(storage)
    , mImageInfoProvider(imageInfoProvider)
    , mAllocator(allocator)
{
    static_cast<InstanceIdent&>(mStatus) = info.mInstanceIdent; // NOSONAR cpp:S3230 - needs Clear() calls below, not
                                                                // just an init-list

    mStatus.mPreinstalled   = false;
    mStatus.mNodeID         = info.mNodeID;
    mStatus.mRuntimeID      = info.mRuntimeID;
    mStatus.mManifestDigest = info.mManifestDigest;
    mStatus.mStateChecksum.Clear();
    mStatus.mEnvVarsStatuses.Clear();
    mStatus.mState = aos::InstanceStateEnum::eActivating;
    mStatus.mError = ErrorEnum::eNone;
}

Error Instance::LoadConfigs(const oci::IndexContentDescriptor& imageDescriptor)
{
    mItemConfig = MakeUnique<oci::ItemConfig>(&mAllocator);
    if (!mItemConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    mImageConfig = MakeUnique<oci::ImageConfig>(&mAllocator);
    if (!mImageConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto releaseConfigs = DeferRelease([this]() { ResetConfigs(); });
    if (auto err = mImageInfoProvider.GetItemConfig(imageDescriptor, *mItemConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "get item config failed"));
    }

    if (auto err = SetDefaultRuntimes(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mImageInfoProvider.GetImageConfig(imageDescriptor, *mImageConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(err, "get image config failed"));
    }

    (void)releaseConfigs.Release();

    mInfo.mManifestDigest = imageDescriptor.mDigest;

    return ErrorEnum::eNone;
}

void Instance::ResetConfigs()
{
    mItemConfig.Reset();
    mImageConfig.Reset();
}

bool Instance::IsImageValid()
{
    // If manifest digest is empty, instance is not running yet, so treat it as valid.
    if (mInfo.mManifestDigest.IsEmpty()) {
        return true;
    }

    auto imageIndex = MakeUnique<oci::ImageIndex>(&mAllocator);
    if (!imageIndex) {
        LOG_ERR() << "Can't allocate image index" << Log::Field(ErrorEnum::eNoMemory);

        return false;
    }

    if (auto err = mImageInfoProvider.GetImageIndex(mInfo.mIndexDigest, *imageIndex); !err.IsNone()) {
        return false;
    }

    auto imageDescriptor = imageIndex->mManifests.FindIf(
        [&](const oci::IndexContentDescriptor& descriptor) { return descriptor.mDigest == mInfo.mManifestDigest; });

    return imageDescriptor != imageIndex->mManifests.end();
}

Error Instance::UpdateStatus(const InstanceStatus& status)
{
    mStatus = status;

    if (auto err = mStorage.UpdateInstance(mInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void Instance::SetError(const Error& err, bool resetNodeID)
{
    if (resetNodeID) {
        mInfo.mPrevNodeID = mInfo.mNodeID;
        mInfo.mNodeID     = "";
        mInfo.mRuntimeID  = "";

        mStatus.mNodeID    = "";
        mStatus.mRuntimeID = "";
    }

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

bool Instance::IsRuntimeTypeOk(
    const StaticString<cRuntimeTypeLen>& runtimeType, const StaticString<cIDLen>& runtimeID) const
{
    assert(mItemConfig);

    return (mInfo.mDisableRebalancing && mInfo.mRuntimeID == runtimeID)
        || (!mInfo.mDisableRebalancing && mItemConfig->mRuntimes.Contains(runtimeType));
}

bool Instance::IsPlatformOk(const PlatformInfo& platformInfo) const
{
    assert(mImageConfig);

    // Required params: OS, architecture
    if (platformInfo.mArchInfo.mArchitecture != mImageConfig->mArchitecture) {
        return false;
    }

    // Optional params: architecture variant, OS, OS version, OS features

    if (!mImageConfig->mOS.IsEmpty() && platformInfo.mOSInfo.mOS != mImageConfig->mOS) {
        return false;
    }

    if (!mImageConfig->mVariant.IsEmpty() && platformInfo.mArchInfo.mVariant != mImageConfig->mVariant) {
        return false;
    }

    if (!mImageConfig->mOSVersion.IsEmpty() && platformInfo.mOSInfo.mVersion != mImageConfig->mOSVersion) {
        return false;
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

bool Instance::IsNodeIDOk(const String& nodeID) const
{
    return !mInfo.mDisableRebalancing || mInfo.mNodeID == nodeID;
}

bool Instance::AreNodeLabelsOk(const LabelsArray& nodeLabels) const
{
    for (const auto& label : mInfo.mLabels) {
        if (!nodeLabels.Contains(label)) {
            return false;
        }
    }

    return true;
}

RetWithError<bool> Instance::OverrideEnvVars(const OverrideEnvVarsRequest& envVars)
{
    auto newEnvVars = MakeUnique<EnvVarArray>(&mAllocator);
    if (!newEnvVars) {
        return {false, AOS_ERROR_WRAP(ErrorEnum::eNoMemory)};
    }

    for (const auto& item : envVars.mItems) {
        if (!item.Match(mInfo.mInstanceIdent)) {
            continue;
        }

        for (const auto& overrideVar : item.mVariables) {
            auto existingVar = newEnvVars->FindIf(
                [&overrideVar](const EnvVar& envVar) { return envVar.mName == overrideVar.mName; });

            if (existingVar != newEnvVars->end()) {
                existingVar->mValue = overrideVar.mValue;
            } else {
                if (auto err = newEnvVars->PushBack(static_cast<const EnvVar&>(overrideVar)); !err.IsNone()) {
                    return {false, AOS_ERROR_WRAP(err)};
                }
            }
        }
    }

    bool changed = *newEnvVars != mSMInfo.mEnvVars;
    if (changed) {
        mSMInfo.mEnvVars = *newEnvVars;
    }

    return {changed, ErrorEnum::eNone};
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

Error Instance::SetDefaultRuntimes()
{
    if (!mItemConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
    }

    if (auto& runtimes = mItemConfig->mRuntimes; runtimes.IsEmpty()) {
        static constexpr const char* const cDefaultRuntimes[] = {
            "crun",
            "runc",
        };

        for (const auto& runtime : cDefaultRuntimes) {
            if (auto err = runtimes.EmplaceBack(); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            if (auto err = runtimes.Back().Assign(runtime); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * ComponentInstance implementation
 **********************************************************************************************************************/

ComponentInstance::ComponentInstance(
    AllocatorItf& allocator, const InstanceInfo& info, StorageItf& storage, ImageInfoProvider& imageInfoProvider)
    : Instance(allocator, info, storage, imageInfoProvider)
{
}

Error ComponentInstance::Init()
{
    return ErrorEnum::eNone;
}

Error ComponentInstance::Remove()
{
    LOG_DBG() << "Remove instance" << Log::Field("instanceID", GetInfo().mInstanceIdent);

    if (auto err = GetStorage().RemoveInstance(GetInfo().mInstanceIdent);
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ComponentInstance::Cache(bool disable)
{
    LOG_DBG() << "Cache instance" << Log::Field("instanceID", GetInfo().mInstanceIdent)
              << Log::Field("disable", disable);

    GetInstanceInfo().mState  = disable ? InstanceStateEnum::eDisabled : InstanceStateEnum::eCached;
    GetInstanceInfo().mNodeID = "";

    if (auto err = GetStorage().UpdateInstance(GetInfo()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool ComponentInstance::IsAvailableCpuOk(size_t availableCPU, const NodeItf& node)
{
    (void)availableCPU;
    (void)node;

    return true;
}

bool ComponentInstance::IsAvailableRamOk(size_t availableRAM, const NodeItf& node)
{
    (void)availableRAM;
    (void)node;

    return true;
}

bool ComponentInstance::AreNodeResourcesOk(const NodeItf& node)
{
    (void)node;

    return true;
}

oci::BalancingPolicyEnum ComponentInstance::GetBalancingPolicy()
{
    return oci::BalancingPolicyEnum::eDisabled;
}

Error ComponentInstance::Schedule(NodeItf& node, const String& runtimeID)
{
    return LoadSMInfo(node, runtimeID);
}

Error ComponentInstance::LoadSMInfo(NodeItf& node, const String& runtimeID)
{
    static_cast<InstanceIdent&>(GetSMInstanceInfo()) = GetInfo().mInstanceIdent;
    GetSMInstanceInfo().mManifestDigest              = GetInfo().mManifestDigest;
    GetSMInstanceInfo().mRuntimeID                   = runtimeID;
    GetSMInstanceInfo().mOwnerID                     = GetInfo().mOwnerID;
    GetSMInstanceInfo().mSubjectType                 = GetInfo().mSubjectType;
    GetSMInstanceInfo().mUID                         = GetInfo().mUID;
    GetSMInstanceInfo().mGID                         = GetInfo().mGID;
    GetSMInstanceInfo().mPriority                    = GetInfo().mPriority;

    GetSMInstanceInfo().mStoragePath = "";
    GetSMInstanceInfo().mStatePath   = "";
    // mSMInfo.mEnvVars is set in OverrideEnvVars().
    GetSMInstanceInfo().mMonitoringParams.Reset();

    if (auto err = SetActive(node.GetConfig().mNodeID, runtimeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * ServiceInstance implementation
 **********************************************************************************************************************/

ServiceInstance::ServiceInstance(AllocatorItf& allocator, const InstanceInfo& info, UIDPool& uidPool, GIDPool& gidPool,
    StorageItf& storage, StorageState& storageState, ImageInfoProvider& imageInfoProvider)
    : Instance(allocator, info, storage, imageInfoProvider)
    , mUIDPool(uidPool)
    , mGIDPool(gidPool)
    , mStorageState(storageState)
{
}

Error ServiceInstance::Init()
{
    Error uidErr;
    Error gidErr;

    Tie(GetInstanceInfo().mUID, uidErr) = mUIDPool.Acquire(GetUIDKey(GetInfo().mInstanceIdent), GetInfo().mUID);
    if (!uidErr.IsNone()) {
        return AOS_ERROR_WRAP(uidErr);
    }

    Tie(GetInstanceInfo().mGID, gidErr) = mGIDPool.Acquire(GetInfo().mInstanceIdent.mItemID, GetInfo().mGID);
    if (!gidErr.IsNone()) {
        return AOS_ERROR_WRAP(gidErr);
    }

    return ErrorEnum::eNone;
}

Error ServiceInstance::Remove()
{
    LOG_DBG() << "Remove instance" << Log::Field("instanceID", GetInfo().mInstanceIdent);

    Error firstErr;
    if (auto err = mStorageState.Remove(GetInfo().mInstanceIdent); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        firstErr = AOS_ERROR_WRAP(err);
    }

    if (auto err = GetStorage().RemoveInstance(GetInfo().mInstanceIdent);
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound) && firstErr.IsNone()) {
        firstErr = AOS_ERROR_WRAP(err);
    }

    if (auto err = mUIDPool.Release(GetUIDKey(GetInfo().mInstanceIdent));
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound) && firstErr.IsNone()) {
        firstErr = AOS_ERROR_WRAP(err);
    }

    if (auto err = mGIDPool.Release(GetInfo().mInstanceIdent.mItemID);
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound) && firstErr.IsNone()) {
        firstErr = AOS_ERROR_WRAP(err);
    }

    return firstErr;
}

Error ServiceInstance::Cache(bool disable)
{
    LOG_DBG() << "Cache instance" << Log::Field("instanceID", GetInfo().mInstanceIdent)
              << Log::Field("disable", disable);

    GetInstanceInfo().mState  = disable ? InstanceStateEnum::eDisabled : InstanceStateEnum::eCached;
    GetInstanceInfo().mNodeID = "";

    if (auto err = GetStorage().UpdateInstance(GetInfo()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorageState.Cleanup(GetInfo().mInstanceIdent); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool ServiceInstance::IsAvailableCpuOk(size_t availableCPU, const NodeItf& node)
{
    assert(GetItemConfig());

    auto requestedCPU = GetRequestedCPU(node);

    bool ok = availableCPU >= requestedCPU;

    LOG_DBG() << "Available CPU " << (ok ? "enough" : "not enough") << Log::Field("nodeID", node.GetConfig().mNodeID)
              << Log::Field("availableCPU", availableCPU) << Log::Field("requestedCPU", requestedCPU);

    return ok;
}

bool ServiceInstance::IsAvailableRamOk(size_t availableRAM, const NodeItf& node)
{
    assert(GetItemConfig());

    auto requestedRAM = GetRequestedRAM(node);

    bool ok = availableRAM >= requestedRAM;

    LOG_DBG() << "Available RAM " << (ok ? "enough" : "not enough") << Log::Field("nodeID", node.GetConfig().mNodeID)
              << Log::Field("availableRAM", availableRAM) << Log::Field("requestedRAM", requestedRAM);

    return ok;
}

bool ServiceInstance::AreNodeResourcesOk(const NodeItf& node)
{
    assert(GetItemConfig());

    for (const auto& resource : GetItemConfig()->mResources) {
        if (node.GetAvailableResourceCount(resource.mName) == 0) {
            return false;
        }
    }

    return true;
}

oci::BalancingPolicyEnum ServiceInstance::GetBalancingPolicy()
{
    assert(GetItemConfig());

    return GetItemConfig()->mBalancingPolicy;
}

Error ServiceInstance::Schedule(NodeItf& node, const String& runtimeID)
{
    assert(GetItemConfig());

    if (auto err = ReserveRuntimeResources(node, runtimeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = LoadSMInfo(node, runtimeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ServiceInstance::LoadSMInfo(NodeItf& node, const String& runtimeID)
{
    assert(GetItemConfig());

    static_cast<InstanceIdent&>(GetSMInstanceInfo()) = GetInfo().mInstanceIdent;
    GetSMInstanceInfo().mManifestDigest              = GetInfo().mManifestDigest;
    GetSMInstanceInfo().mRuntimeID                   = runtimeID;
    GetSMInstanceInfo().mOwnerID                     = GetInfo().mOwnerID;
    GetSMInstanceInfo().mSubjectType                 = GetInfo().mSubjectType;
    GetSMInstanceInfo().mUID                         = GetInfo().mUID;
    GetSMInstanceInfo().mGID                         = GetInfo().mGID;
    GetSMInstanceInfo().mPriority                    = GetInfo().mPriority;

    if (auto err
        = SetupStateStorage(node.GetConfig(), GetSMInstanceInfo().mStoragePath, GetSMInstanceInfo().mStatePath);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // mSMInfo.mEnvVars is set in OverrideEnvVars().

    GetSMInstanceInfo().mMonitoringParams.EmplaceValue();
    if (GetItemConfig()->mAlertRules.HasValue()) {
        GetSMInstanceInfo().mMonitoringParams.GetValue().mAlertRules = GetItemConfig()->mAlertRules.GetValue();
    }

    if (auto err = SetActive(node.GetConfig().mNodeID, runtimeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

size_t ServiceInstance::GetRequestedCPU(const NodeItf& node)
{
    assert(GetItemConfig());

    if (GetItemConfig()->mSkipResourceLimits) {
        return 0;
    }

    const auto& nodeConfig = node.GetConfig();

    size_t requestedCPU = 0;
    auto   quota        = GetItemConfig()->mQuotas.mCPUDMIPSLimit;

    if (GetItemConfig()->mRequestedResources.HasValue() && GetItemConfig()->mRequestedResources->mCPU.HasValue()) {
        requestedCPU = ClampResource(*GetItemConfig()->mRequestedResources->mCPU, quota);
    } else {
        requestedCPU = GetReqCPUFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    if (node.NeedBalancing() && GetMonitoringData().mCPU > static_cast<double>(requestedCPU)) {
        return static_cast<size_t>(GetMonitoringData().mCPU);
    }

    return requestedCPU;
}

size_t ServiceInstance::GetRequestedRAM(const NodeItf& node)
{
    assert(GetItemConfig());

    if (GetItemConfig()->mSkipResourceLimits) {
        return 0;
    }

    const auto& nodeConfig = node.GetConfig();

    size_t requestedRAM = 0;
    auto   quota        = GetItemConfig()->mQuotas.mRAMLimit;

    if (GetItemConfig()->mRequestedResources.HasValue() && GetItemConfig()->mRequestedResources->mRAM.HasValue()) {
        requestedRAM = ClampResource(*GetItemConfig()->mRequestedResources->mRAM, quota);
    } else {
        requestedRAM = GetReqRAMFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    if (node.NeedBalancing() && GetMonitoringData().mRAM > requestedRAM) {
        return GetMonitoringData().mRAM;
    }

    return requestedRAM;
}

size_t ServiceInstance::GetReqStateSize(const NodeConfig& nodeConfig)
{
    size_t requestedState = 0;
    auto   quota          = GetItemConfig()->mQuotas.mStateLimit;

    if (GetItemConfig()->mRequestedResources.HasValue() && GetItemConfig()->mRequestedResources->mState.HasValue()) {
        requestedState = ClampResource(*GetItemConfig()->mRequestedResources->mState, quota);
    } else {
        requestedState = GetReqStateFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    return requestedState;
}

size_t ServiceInstance::GetReqStorageSize(const NodeConfig& nodeConfig)
{
    size_t requestedStorage = 0;
    auto   quota            = GetItemConfig()->mQuotas.mStorageLimit;

    if (GetItemConfig()->mRequestedResources.HasValue() && GetItemConfig()->mRequestedResources->mStorage.HasValue()) {
        requestedStorage = ClampResource(*GetItemConfig()->mRequestedResources->mStorage, quota);
    } else {
        requestedStorage = GetReqStorageFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    return requestedStorage;
}

size_t ServiceInstance::ClampResource(size_t value, const Optional<size_t>& quota) const
{
    if (quota.HasValue() && value > quota.GetValue()) {
        return quota.GetValue();
    }

    return value;
}

size_t ServiceInstance::GetReqCPUFromNodeConfig(
    const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios) const
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mCPU.HasValue()) {
        ratio = nodeRatios->mCPU.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<size_t>(static_cast<double>(*quota) * ratio + 0.5);
    }

    return 0;
}

size_t ServiceInstance::GetReqRAMFromNodeConfig(
    const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios) const
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mRAM.HasValue()) {
        ratio = nodeRatios->mRAM.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<size_t>(static_cast<double>(*quota) * ratio + 0.5);
    }

    return 0;
}

size_t ServiceInstance::GetReqStateFromNodeConfig(
    const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios) const
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mState.HasValue()) {
        ratio = nodeRatios->mState.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<size_t>(static_cast<double>(*quota) * ratio + 0.5);
    }

    return 0;
}

size_t ServiceInstance::GetReqStorageFromNodeConfig(
    const Optional<size_t>& quota, const Optional<ResourceRatios>& nodeRatios) const
{
    auto ratio = cDefaultResourceRation / 100.0;

    if (nodeRatios.HasValue() && nodeRatios->mStorage.HasValue()) {
        ratio = nodeRatios->mStorage.GetValue() / 100.0;
    }

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (quota.HasValue()) {
        return static_cast<size_t>(static_cast<double>(*quota) * ratio + 0.5);
    }

    return 0;
}

Error ServiceInstance::SetupStateStorage(const NodeConfig& nodeConfig, String& storagePath, String& statePath)
{
    auto reqState   = GetReqStateSize(nodeConfig);
    auto reqStorage = GetReqStorageSize(nodeConfig);

    storagestate::SetupParams params;

    params.mUID = GetInfo().mUID;
    params.mGID = GetInfo().mGID;

    if (GetItemConfig()->mQuotas.mStorageLimit.HasValue()) {
        params.mStorageQuota = *GetItemConfig()->mQuotas.mStorageLimit;
    }

    if (GetItemConfig()->mQuotas.mStateLimit.HasValue()) {
        params.mStateQuota = *GetItemConfig()->mQuotas.mStateLimit;
    }

    if (GetItemConfig()->mSkipResourceLimits) {
        reqState   = 0;
        reqStorage = 0;
    }

    if (auto err = mStorageState.SetupStateStorage(
            GetInfo().mInstanceIdent, params, reqStorage, reqState, storagePath, statePath);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ServiceInstance::ReserveRuntimeResources(NodeItf& node, const String& runtimeID)
{
    auto                     requestedCPU       = GetItemConfig()->mSkipResourceLimits ? 0 : GetRequestedCPU(node);
    auto                     requestedRAM       = GetItemConfig()->mSkipResourceLimits ? 0 : GetRequestedRAM(node);
    Array<oci::ResourceInfo> requestedResources = GetItemConfig()->mSkipResourceLimits
        ? Array<oci::ResourceInfo>()
        : GetItemConfig()->mResources; // NOSONAR cpp:S5912 - Array serves as a view over StaticArray

    if (auto reserveErr
        = node.ReserveResources(GetInfo().mInstanceIdent, runtimeID, requestedCPU, requestedRAM, requestedResources);
        !reserveErr.IsNone()) {
        return AOS_ERROR_WRAP(reserveErr);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
