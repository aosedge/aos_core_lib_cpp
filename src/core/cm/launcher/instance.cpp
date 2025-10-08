/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "instance.hpp"

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Instance implementation
 **********************************************************************************************************************/

Instance::Instance(const InstanceInfo& info, StorageItf& storage, ImageInfoProviderItf& imageInfoProvider)
    : mInfo(info)
    , mStorage(storage)
    , mImageInfoProvider(imageInfoProvider)
{
    static_cast<InstanceIdent&>(mStatus) = info.mInstanceIdent;
    mStatus.mError                       = ErrorEnum::eNone;
    mStatus.mNodeID                      = info.mNodeID;
    mStatus.mRuntimeID                   = info.mRuntimeID;
}

bool Instance::IsImageValid()
{
    auto config = MakeShared<oci::ServiceConfig>(&mAllocator);
    auto images = MakeShared<StaticArray<aos::ImageInfo, cMaxNumUpdateImages>>(&mAllocator);

    if (auto err = mImageInfoProvider.GetItemImages(mInfo.mInstanceIdent.mItemID, *images); !err.IsNone()) {
        return false;
    }

    for (const auto& image : *images) {
        auto servErr = mImageInfoProvider.GetServiceConfig(mInfo.mInstanceIdent.mItemID, image.mImageID, *config);
        if (!servErr.IsNone()) {
            return false;
        }
    }

    return true;
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

Error Instance::Schedule(const aos::InstanceInfo& info, const String& nodeID)
{
    LOG_DBG() << "Schedule instance" << Log::Field("instanceID", static_cast<const InstanceIdent&>(info))
              << Log::Field("nodeID", nodeID);

    mInfo.mInstanceIdent = static_cast<const InstanceIdent&>(info);
    mInfo.mImageID       = info.mImageID;
    mInfo.mRuntimeID     = info.mRuntimeID;
    mInfo.mPrevNodeID    = mInfo.mNodeID;
    mInfo.mNodeID        = nodeID;
    mInfo.mUID           = info.mUID;
    mInfo.mTimestamp     = Time::Now();
    mInfo.mCached        = false;

    static_cast<InstanceIdent&>(mStatus) = static_cast<const InstanceIdent&>(info);
    mStatus.mNodeID                      = nodeID;
    mStatus.mRuntimeID                   = info.mRuntimeID;
    mStatus.mState                       = InstanceStateEnum::eActivating;
    mStatus.mError                       = ErrorEnum::eNone;

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
    mStatus.mState = InstanceStateEnum::eFailed;

    if (auto updateErr = mStorage.UpdateInstance(mInfo); !updateErr.IsNone()) {
        LOG_ERR() << "Can't set instance error status" << Log::Field(err);
    }
}

void Instance::UpdateMonitoringData(const MonitoringData& monitoringData)
{
    mMonitoringData = monitoringData;
}

/***********************************************************************************************************************
 * ComponentInstance implementation
 **********************************************************************************************************************/

ComponentInstance::ComponentInstance(
    const InstanceInfo& info, StorageItf& storage, ImageInfoProviderItf& imageInfoProvider)
    : Instance(info, storage, imageInfoProvider)
{
}

Error ComponentInstance::Init()
{
    return ErrorEnum::eNone;
}

Error ComponentInstance::Remove()
{
    LOG_DBG() << "Remove instance" << Log::Field("instanceID", mInfo.mInstanceIdent);

    if (auto err = mStorage.RemoveInstance(mInfo.mInstanceIdent); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ComponentInstance::Cache()
{
    LOG_DBG() << "Cache instance" << Log::Field("instanceID", mInfo.mInstanceIdent);

    mInfo.mCached = true;
    mInfo.mNodeID = "";

    if (auto err = mStorage.UpdateInstance(mInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

size_t ComponentInstance::GetRequestedCPU(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig)
{
    (void)nodeConfig;
    (void)serviceConfig;

    return 0;
}

size_t ComponentInstance::GetRequestedRAM(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig)
{
    (void)nodeConfig;
    (void)serviceConfig;

    return 0;
}

Error ComponentInstance::GetServiceConfig(const String& imageID, oci::ServiceConfig& config)
{
    (void)imageID;

    static oci::ServiceConfig cDefaultServiceCfg;

    config = cDefaultServiceCfg;
    config.mRunners.PushBack("rootfs");

    return ErrorEnum::eNone;
}

Error ComponentInstance::GetImageConfig(const String& imageID, oci::ImageConfig& config)
{
    (void)imageID;

    static oci::ImageConfig cDefaultImageCfg;

    config = cDefaultImageCfg;

    return ErrorEnum::eNone;
}

Error ComponentInstance::GetItemImages(Array<ImageInfo>& imagesInfos)
{
    static ImageInfo cDefaultImageInfo;

    cDefaultImageInfo.mImageID = "rootfs";

    imagesInfos.Clear();
    if (auto err = imagesInfos.PushBack(cDefaultImageInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * ServiceInstance implementation
 **********************************************************************************************************************/

ServiceInstance::ServiceInstance(const InstanceInfo& info, UIDPool& uidPool, StorageItf& storage,
    storagestate::StorageStateItf& storageState, ImageInfoProviderItf& imageInfoProvider)
    : Instance(info, storage, imageInfoProvider)
    , mUIDPool(uidPool)
    , mStorageState(storageState)
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

    return ErrorEnum::eNone;
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

    return ErrorEnum::eNone;
}

Error ServiceInstance::Cache()
{
    LOG_DBG() << "Cache instance" << Log::Field("instanceID", mInfo.mInstanceIdent);

    mInfo.mCached = true;
    mInfo.mNodeID = "";

    if (auto err = mStorage.UpdateInstance(mInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorageState.Cleanup(mInfo.mInstanceIdent); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

size_t ServiceInstance::GetRequestedCPU(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig)
{
    size_t requestedCPU = 0;
    auto   quota        = serviceConfig.mQuotas.mCPUDMIPSLimit;

    if (serviceConfig.mRequestedResources.HasValue() && serviceConfig.mRequestedResources->mCPU.HasValue()) {
        requestedCPU = ClampResource(*serviceConfig.mRequestedResources->mCPU, quota);
    } else {
        requestedCPU = GetReqCPUFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    return requestedCPU;
}

size_t ServiceInstance::GetRequestedRAM(const NodeConfig& nodeConfig, const oci::ServiceConfig& serviceConfig)
{
    size_t requestedRAM = 0;
    auto   quota        = serviceConfig.mQuotas.mRAMLimit;

    if (serviceConfig.mRequestedResources.HasValue() && serviceConfig.mRequestedResources->mRAM.HasValue()) {
        requestedRAM = ClampResource(*serviceConfig.mRequestedResources->mRAM, quota);
    } else {
        requestedRAM = GetReqRAMFromNodeConfig(quota, nodeConfig.mResourceRatios);
    }

    return requestedRAM;
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

Error ServiceInstance::GetServiceConfig(const String& imageID, oci::ServiceConfig& config)
{
    return mImageInfoProvider.GetServiceConfig(mInfo.mInstanceIdent.mItemID, imageID, config);
}

Error ServiceInstance::GetImageConfig(const String& imageID, oci::ImageConfig& config)
{
    // Service instances get real configuration data from ImageInfoProvider
    return mImageInfoProvider.GetImageConfig(mInfo.mInstanceIdent.mItemID, imageID, config);
}

Error ServiceInstance::GetItemImages(Array<ImageInfo>& imagesInfos)
{
    return mImageInfoProvider.GetItemImages(mInfo.mInstanceIdent.mItemID, imagesInfos);
}

} // namespace aos::cm::launcher
