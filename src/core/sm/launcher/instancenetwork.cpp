/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "instancenetwork.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void InstanceNetwork::Init(AllocatorItf& allocator, networkmanager::NetworkManagerItf& networkManager,
    resourcemanager::ResourceInfoProviderItf& resourceInfoProvider)
{
    mAllocator            = &allocator;
    mNetworkManager       = &networkManager;
    mResourceInfoProvider = &resourceInfoProvider;
}

Error InstanceNetwork::Create(
    const InstanceData& instance, const oci::ItemConfig& itemConfig, const oci::ImageConfig& imageConfig)
{
    auto networkConfig = MakeUnique<networkmanager::InstanceNetworkConfig>(mAllocator);
    if (!networkConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = GetNetworkConfig(instance.mInfo, itemConfig, imageConfig, *networkConfig); !err.IsNone()) {
        return err;
    }

    if (auto err
        = mNetworkManager->CreateInstanceNetwork(instance.mInstanceID, instance.mInfo.mOwnerID, *networkConfig);
        !err.IsNone() && !err.Is(ErrorEnum::eAlreadyExist)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceNetwork::Start(const InstanceData& instance)
{
    if (auto err = mNetworkManager->StartInstanceNetwork(instance.mInstanceID, instance.mInfo.mOwnerID);
        !err.IsNone() && !err.Is(ErrorEnum::eAlreadyExist)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceNetwork::Stop(const InstanceData& instance)
{
    if (auto err = mNetworkManager->StopInstanceNetwork(instance.mInstanceID, instance.mInfo.mOwnerID);
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceNetwork::Release(const InstanceData& instance)
{
    if (auto err = mNetworkManager->ReleaseInstanceNetwork(instance.mInstanceID, instance.mInfo.mOwnerID);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceNetwork::BeginBatch()
{
    if (auto err = mNetworkManager->BeginBatch(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceNetwork::FlushBatch(Array<StaticString<cIDLen>>& failedInstanceIDs)
{
    if (auto err = mNetworkManager->FlushBatch(failedInstanceIDs); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceNetwork::GetNetworkConfig(const InstanceInfo& instance, const oci::ItemConfig& itemConfig,
    const oci::ImageConfig& imageConfig, networkmanager::InstanceNetworkConfig& networkConfig)
{
    networkConfig.mInstanceIdent = static_cast<const InstanceIdent&>(instance);

    auto resourceInfo = MakeUnique<resourcemanager::ResourceInfo>(mAllocator);
    if (!resourceInfo) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    for (const auto& resource : itemConfig.mResources) {
        if (auto err = mResourceInfoProvider->GetResourceInfo(resource.mName, *resourceInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = networkConfig.mHosts.Insert(
                networkConfig.mHosts.end(), resourceInfo->mHosts.begin(), resourceInfo->mHosts.end());
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (itemConfig.mHostname.HasValue()) {
        networkConfig.mHostname = *itemConfig.mHostname;
    }

    if (itemConfig.mQuotas.mDownloadSpeed.HasValue()) {
        networkConfig.mIngressKbit = *itemConfig.mQuotas.mDownloadSpeed;
    }

    if (itemConfig.mQuotas.mUploadSpeed.HasValue()) {
        networkConfig.mEgressKbit = *itemConfig.mQuotas.mUploadSpeed;
    }

    if (itemConfig.mQuotas.mDownloadLimit.HasValue()) {
        networkConfig.mDownloadLimit = *itemConfig.mQuotas.mDownloadLimit;
    }

    if (itemConfig.mQuotas.mUploadLimit.HasValue()) {
        networkConfig.mUploadLimit = *itemConfig.mQuotas.mUploadLimit;
    }

    if (auto err = networkConfig.mExposedPorts.Assign(imageConfig.mConfig.mExposedPorts); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = networkConfig.mAllowedConnections.Assign(itemConfig.mAllowedConnections); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
