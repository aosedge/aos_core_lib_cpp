/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "instancelauncher.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void InstanceLauncher::Init(AllocatorItf& allocator, RuntimeManager& runtimes, StorageItf& storage,
    common::imagemanager::ItemInfoProviderItf& itemInfoProvider, oci::OCISpecItf& ociSpec,
    networkmanager::NetworkManagerItf& networkManager, resourcemanager::ResourceInfoProviderItf& resourceInfoProvider)
{
    mAllocator = &allocator;
    mRuntimes  = &runtimes;
    mStorage   = &storage;

    mItemConfigLoader.Init(allocator, itemInfoProvider, ociSpec);
    mNetwork.Init(allocator, networkManager, resourceInfoProvider);
}

Error InstanceLauncher::LoadConfigs(InstanceData& instance)
{
    auto itemConfig = MakeUnique<oci::ItemConfig>(mAllocator);
    if (!itemConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto imageConfig = MakeUnique<oci::ImageConfig>(mAllocator);
    if (!imageConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    return LoadConfigs(instance, *itemConfig, *imageConfig);
}

Error InstanceLauncher::Prepare(InstanceData& instance)
{
    LOG_DBG() << "Prepare instance" << Log::Field("instance", instance.mInfo);

    if (auto err = mStorage->UpdateInstanceInfo(instance.mInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (instance.mInfo.mType != UpdateItemTypeEnum::eService) {
        return ErrorEnum::eNone;
    }

    auto itemConfig = MakeUnique<oci::ItemConfig>(mAllocator);
    if (!itemConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    auto imageConfig = MakeUnique<oci::ImageConfig>(mAllocator);
    if (!imageConfig) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = LoadConfigs(instance, *itemConfig, *imageConfig); !err.IsNone()) {
        return err;
    }

    if (auto err = mNetwork.Create(instance, *itemConfig, *imageConfig); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error InstanceLauncher::StartNetwork(InstanceData& instance)
{
    return mNetwork.Start(instance);
}

Error InstanceLauncher::Start(InstanceData& instance)
{
    auto* runtime = mRuntimes->Find(instance.mInfo.mRuntimeID);
    if (runtime == nullptr) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "runtime not found"));
    }

    // The configs of the instance restored on module start aren't loaded if their read failed. Only the broken content
    // fails the start: the other errors are reported by the runtime.
    if (instance.mInfo.mType == UpdateItemTypeEnum::eService && !instance.mConfigsLoaded) {
        if (auto err = LoadConfigs(instance); err.Is(ErrorEnum::eIntegrity)) {
            return err;
        }
    }

    LOG_INF() << "Start instance" << Log::Field("instance", instance.mInfo)
              << Log::Field("runtimeID", instance.mInfo.mRuntimeID)
              << Log::Field("manifestDigest", instance.mInfo.mManifestDigest);

    if (auto err = runtime->StartInstance(instance.mInfo, instance.mStatus);
        !err.IsNone() && !err.Is(ErrorEnum::eAlreadyExist)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceLauncher::Stop(InstanceData& instance)
{
    auto* runtime = mRuntimes->Find(instance.mStatus.mRuntimeID);
    if (runtime == nullptr) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "runtime not found"));
    }

    LOG_INF() << "Stop instance" << Log::Field("instance", instance.mInfo)
              << Log::Field("runtimeID", instance.mInfo.mRuntimeID);

    if (auto err = runtime->StopInstance(instance.mInfo, instance.mStatus);
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceLauncher::StopNetwork(InstanceData& instance)
{
    return mNetwork.Stop(instance);
}

Error InstanceLauncher::Release(const InstanceData& instance)
{
    LOG_DBG() << "Remove instance" << Log::Field("instance", instance.mInfo);

    if (auto err = mStorage->RemoveInstanceInfo(instance.mInfo); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNetwork.Release(instance); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceLauncher::BeginNetworkBatch()
{
    return mNetwork.BeginBatch();
}

Error InstanceLauncher::FlushNetworkBatch(Array<StaticString<cIDLen>>& failedInstanceIDs)
{
    return mNetwork.FlushBatch(failedInstanceIDs);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error InstanceLauncher::LoadConfigs(InstanceData& instance, oci::ItemConfig& itemConfig, oci::ImageConfig& imageConfig)
{
    if (auto err = mItemConfigLoader.Load(instance.mInfo.mManifestDigest, itemConfig, imageConfig); !err.IsNone()) {
        return err;
    }

    instance.mOfflineTTL    = itemConfig.mOfflineTTL;
    instance.mConfigsLoaded = true;

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
