/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "instancerunner.hpp"

namespace aos::sm::launcher {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

bool IsService(const InstanceData& instance)
{
    return instance.mInfo.mType == UpdateItemTypeEnum::eService;
}

bool IsFailed(const InstanceData& instance)
{
    return instance.mStatus.mState == InstanceStateEnum::eFailed;
}

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void InstanceRunner::Init(AllocatorItf& allocator, RuntimeManager& runtimes, StorageItf& storage,
    common::imagemanager::ItemInfoProviderItf& itemInfoProvider, oci::OCISpecItf& ociSpec,
    networkmanager::NetworkManagerItf& networkManager, resourcemanager::ResourceInfoProviderItf& resourceInfoProvider,
    Mutex& stateMutex)
{
    mAllocator  = &allocator;
    mRuntimes   = &runtimes;
    mStorage    = &storage;
    mStateMutex = &stateMutex;

    mItemConfigLoader.Init(allocator, itemInfoProvider, ociSpec);
    mNetwork.Init(allocator, networkManager, resourceInfoProvider);
}

void InstanceRunner::LoadConfigs(const Array<InstanceData*>& instances)
{
    LOG_DBG() << "Load instances configs" << Log::Field("count", instances.Size());

    if (auto err = mPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't start thread pool" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    for (auto* instance : instances) {
        if (!IsService(*instance)) {
            continue;
        }

        if (auto err = mPool.AddTask([this, instance](void*) {
                if (auto err = LoadConfigs(*instance); !err.IsNone()) {
                    LOG_ERR() << "Failed to load instance configs" << Log::Field("instance", instance->mInfo)
                              << Log::Field(err);
                }
            });
            !err.IsNone()) {
            LOG_ERR() << "Failed to load instance configs" << Log::Field("instance", instance->mInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

void InstanceRunner::Start(const Array<InstanceData*>& instances)
{
    LOG_INF() << "[profiling] Start instances begin" << Log::Field("count", instances.Size());

    if (auto err = mPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't start thread pool" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    RunTasks(
        instances, "Failed to prepare instance",
        [](const InstanceData& instance) { return !IsFailed(instance) && !instance.mPrepared; },
        [this](InstanceData& instance) {
            if (auto err = Prepare(instance); !err.IsNone()) {
                return err;
            }

            instance.mPrepared = true;

            return Error(ErrorEnum::eNone);
        });

    RunNetworkBatch(
        instances, "Failed to start network",
        [](const InstanceData& instance) { return IsService(instance) && !IsFailed(instance); },
        [this](InstanceData& instance) { return mNetwork.Start(instance); },
        [this](InstanceData& instance) {
            Fail(instance, "Failed to start network",
                AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "network batch apply failed")));

            if (auto err = mNetwork.Stop(instance); !err.IsNone()) {
                LOG_ERR() << "Failed to stop network" << Log::Field("instance", instance.mInfo) << Log::Field(err);
            }
        });

    RunTasks(
        instances, "Failed to start instance", [](const InstanceData& instance) { return !IsFailed(instance); },
        [this](InstanceData& instance) {
            SetState(instance, InstanceStateEnum::eActivating);

            if (auto err = StartInstance(instance); !err.IsNone()) {
                return err;
            }

            SetState(instance, InstanceStateEnum::eActive);

            return Error(ErrorEnum::eNone);
        });

    if (auto err = mPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_INF() << "[profiling] Start instances end";
}

void InstanceRunner::Stop(const Array<InstanceData*>& instances)
{
    LOG_INF() << "[profiling] Stop instances begin" << Log::Field("count", instances.Size());

    if (auto err = mPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't start thread pool" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    RunTasks(
        instances, "Failed to stop instance", [](const InstanceData&) { return true; },
        [this](InstanceData& instance) {
            if (auto err = StopInstance(instance); !err.IsNone()) {
                return err;
            }

            SetState(instance, InstanceStateEnum::eInactive);

            return Error(ErrorEnum::eNone);
        });

    RunNetworkBatch(
        instances, "Failed to stop network", [](const InstanceData& instance) { return IsService(instance); },
        [this](InstanceData& instance) { return mNetwork.Stop(instance); },
        [](InstanceData& instance) {
            LOG_WRN() << "Network stop batch failed" << Log::Field("instance", instance.mInfo);
        });

    if (auto err = mPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }

    LOG_INF() << "[profiling] Stop instances end";
}

void InstanceRunner::Release(const Array<InstanceData*>& instances)
{
    LOG_DBG() << "Release instances" << Log::Field("count", instances.Size());

    if (auto err = mPool.Run(); !err.IsNone()) {
        LOG_ERR() << "Can't start thread pool" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    RunTasks(
        instances, "Failed to release instance", [](const InstanceData&) { return true; },
        [this](InstanceData& instance) { return ReleaseInstance(instance); });

    if (auto err = mPool.Shutdown(); !err.IsNone()) {
        LOG_ERR() << "Thread pool shutdown failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

template <typename Filter, typename Task>
void InstanceRunner::RunTasks(const Array<InstanceData*>& instances, const char* message, Filter filter, Task task)
{
    for (auto* instance : instances) {
        if (!filter(*instance)) {
            continue;
        }

        if (auto err = mPool.AddTask([this, instance, message, &task](void*) {
                if (auto err = task(*instance); !err.IsNone()) {
                    Fail(*instance, message, err);
                }
            });
            !err.IsNone()) {
            Fail(*instance, message, AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mPool.Wait(); !err.IsNone()) {
        LOG_ERR() << "Thread pool wait failed" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

template <typename Filter, typename Task, typename HandleFailedIDs>
void InstanceRunner::RunNetworkBatch(const Array<InstanceData*>& instances, const char* message, Filter filter,
    Task task, HandleFailedIDs handleFailedIDs)
{
    auto beginErr = mNetwork.BeginBatch();
    if (!beginErr.IsNone()) {
        LOG_ERR() << "Failed to begin network batch" << Log::Field(AOS_ERROR_WRAP(beginErr));
    }

    RunTasks(instances, message, filter, task);

    if (!beginErr.IsNone()) {
        return;
    }

    auto failedIDs = MakeUnique<StaticArray<StaticString<cIDLen>, cMaxNumInstances>>(mAllocator);
    if (!failedIDs) {
        LOG_ERR() << "Failed to allocate failed network IDs" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    if (auto err = mNetwork.FlushBatch(*failedIDs); !err.IsNone()) {
        LOG_ERR() << "Can't flush network batch" << Log::Field(AOS_ERROR_WRAP(err));
    }

    for (const auto& failedID : *failedIDs) {
        auto instance
            = instances.FindIf([&failedID](const InstanceData* instance) { return instance->mInstanceID == failedID; });
        if (instance == instances.end()) {
            continue;
        }

        handleFailedIDs(**instance);
    }
}

Error InstanceRunner::Prepare(InstanceData& instance)
{
    LOG_DBG() << "Prepare instance" << Log::Field("instance", instance.mInfo);

    if (auto err = mStorage->UpdateInstanceInfo(instance.mInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!IsService(instance)) {
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

    return mNetwork.Create(instance, *itemConfig, *imageConfig);
}

Error InstanceRunner::StartInstance(InstanceData& instance)
{
    auto* runtime = mRuntimes->Find(instance.mInfo.mRuntimeID);
    if (runtime == nullptr) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "runtime not found"));
    }

    // The configs of the instance restored on module start aren't loaded if their read failed. Only the broken content
    // fails the start: the other errors are reported by the runtime.
    if (IsService(instance) && !instance.mConfigsLoaded) {
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

Error InstanceRunner::StopInstance(InstanceData& instance)
{
    auto* runtime = mRuntimes->Find(instance.mStatus.mRuntimeID);
    if (runtime == nullptr) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "runtime not found"));
    }

    LOG_INF() << "Stop instance" << Log::Field("instance", instance.mInfo)
              << Log::Field("runtimeID", instance.mStatus.mRuntimeID);

    if (auto err = runtime->StopInstance(instance.mInfo, instance.mStatus);
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceRunner::ReleaseInstance(const InstanceData& instance)
{
    LOG_DBG() << "Release instance" << Log::Field("instance", instance.mInfo);

    if (auto err = mStorage->RemoveInstanceInfo(instance.mInfo); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return mNetwork.Release(instance);
}

Error InstanceRunner::LoadConfigs(InstanceData& instance)
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

Error InstanceRunner::LoadConfigs(InstanceData& instance, oci::ItemConfig& itemConfig, oci::ImageConfig& imageConfig)
{
    if (auto err = mItemConfigLoader.Load(instance.mInfo.mManifestDigest, itemConfig, imageConfig); !err.IsNone()) {
        return err;
    }

    instance.mOfflineTTL    = itemConfig.mOfflineTTL;
    instance.mConfigsLoaded = true;

    return ErrorEnum::eNone;
}

void InstanceRunner::SetState(InstanceData& instance, InstanceState state, const Error& error)
{
    LockGuard lock {*mStateMutex};

    if (instance.mStatus.mState == state) {
        return;
    }

    instance.mStatus.mState = state;
    instance.mStatus.mError = error;
}

void InstanceRunner::Fail(InstanceData& instance, const char* message, const Error& error)
{
    LOG_ERR() << message << Log::Field("instance", instance.mInfo) << Log::Field(error);

    SetState(instance, InstanceStateEnum::eFailed, error);
}

} // namespace aos::sm::launcher
