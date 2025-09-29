/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "instancemanager.hpp"

#include <core/common/tools/logger.hpp>

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void InstanceManager::Init(const Config& config, StorageItf& storage, ImageInfoProviderItf& imageInfoProvider,
    storagestate::StorageStateItf& storageState)
{
    mConfig            = config;
    mStorage           = &storage;
    mImageInfoProvider = &imageInfoProvider;
    mStorageState      = &storageState;
}

Error InstanceManager::Start()
{
    // Init UID pool
    auto allUIDsValid = [](size_t id) {
        (void)id;
        return true;
    };

    if (auto err = mUIDPool.Init(allUIDsValid); !err.IsNone()) {
        return err;
    }

    if (auto err = LoadInstancesFromStorage(); !err.IsNone()) {
        LOG_ERR() << "Can't load instances from storage " << Log::Field(err);

        return err;
    }

    if (auto err = ClearInstancesWithDeletedImages(); !err.IsNone()) {
        LOG_ERR() << "Can't clear instances with deleted service" << Log::Field(err);

        return err;
    }

    if (auto err = RemoveOutdatedInstances(); !err.IsNone()) {
        LOG_ERR() << "Can't remove outdated instances" << Log::Field(err);

        return err;
    }

    auto onCleanTimerTick = [this](void*) {
        if (auto rmErr = RemoveOutdatedInstances(); !rmErr.IsNone()) {
            LOG_ERR() << "Can't remove outdated instances" << Log::Field(rmErr);
        }
    };

    if (auto err = mCleanInstancesTimer.Start(cRemovePeriod, onCleanTimerTick, false); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto onInitTimerExpired = [this](void*) { SetExpiredStatus(); };

    if (auto err = mInitTimer.Start(mConfig.mNodesConnectionTimeout, onInitTimerExpired); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::Stop()
{
    if (auto err = mCleanInstancesTimer.Stop(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mUIDPool.Clear(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Array<SharedPtr<Instance>>& InstanceManager::GetActiveInstances()
{
    return mActiveInstances;
}

Array<SharedPtr<Instance>>& InstanceManager::GetStashInstances()
{
    return mStashInstances;
}

Error InstanceManager::UpdateStatus(const InstanceStatus& status)
{
    auto instance = FindActiveInstance(status);
    if (instance == nullptr) {
        // not expected instance received from SM.
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return (*instance)->UpdateStatus(status);
}

Error InstanceManager::AddInstanceToStash(const InstanceIdent& id, const RunInstanceRequest& request)
{
    auto instance = FindStashInstance(id);
    if (instance != nullptr) {
        return ErrorEnum::eNone;
    }

    instance = FindActiveInstance(id);
    if (instance != nullptr) {
        if (auto err = mStashInstances.PushBack(*instance); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    auto instanceInfo = MakeUnique<InstanceInfo>(&mAllocator);

    instanceInfo->mInstanceIdent  = id;
    instanceInfo->mUpdateItemType = request.mItemType;
    instanceInfo->mTimestamp      = Time::Now();

    if (auto err = mStorage->AddInstance(*instanceInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto [newInstance, createErr] = CreateInstance(*instanceInfo);
    if (!createErr.IsNone()) {
        return createErr;
    }

    newInstance->SetProviderID(request.mProviderID);

    if (auto err = mStashInstances.PushBack(newInstance); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::SubmitStash()
{
    for (auto& instance : mActiveInstances) {
        auto isStashed = mStashInstances.ContainsIf(
            [&instance](const SharedPtr<Instance>& item) { return instance.Get() == item.Get(); });

        // cache deleted instances
        if (!isStashed) {
            if (auto err = instance->Cache(); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            mCachedInstances.PushBack(instance);
        }
    }

    mActiveInstances = mStashInstances;
    mStashInstances.Clear();

    return ErrorEnum::eNone;
}

SharedPtr<Instance>* InstanceManager::FindActiveInstance(const InstanceIdent& id)
{
    auto instance = mActiveInstances.FindIf(
        [&id](SharedPtr<Instance>& instance) { return instance->GetInfo().mInstanceIdent == id; });

    return instance != mActiveInstances.end() ? instance : nullptr;
}

SharedPtr<Instance>* InstanceManager::FindStashInstance(const InstanceIdent& id)
{
    auto instance = mStashInstances.FindIf(
        [&id](SharedPtr<Instance>& instance) { return instance->GetInfo().mInstanceIdent == id; });

    return instance != mStashInstances.end() ? instance : nullptr;
}

void InstanceManager::UpdateMonitoringData(const Array<monitoring::InstanceMonitoringData>& monitoringData)
{
    for (const auto& instanceData : monitoringData) {
        auto instance = FindActiveInstance(instanceData.mInstanceIdent);
        if (instance) {
            (*instance)->UpdateMonitoringData(instanceData.mMonitoringData);
        }
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error InstanceManager::LoadInstancesFromStorage()
{
    mActiveInstances.Clear();
    mCachedInstances.Clear();

    auto instances = MakeUnique<StaticArray<InstanceInfo, cMaxNumInstances>>(&mAllocator);
    if (auto err = mStorage->GetActiveInstances(*instances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& instance : *instances) {
        if (auto err = LoadInstanceFromStorage(instance); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::LoadInstanceFromStorage(const InstanceInfo& info)
{
    auto [instance, createErr] = CreateInstance(info);
    if (!createErr.IsNone()) {
        return createErr;
    }

    if (!info.mCached) {
        if (auto err = mActiveInstances.EmplaceBack(instance); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        if (auto err = mCachedInstances.EmplaceBack(instance); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::SetExpiredStatus()
{
    for (auto& instance : mActiveInstances) {
        if (instance->GetStatus().mState == InstanceStateEnum::eActivating) {
            instance->SetError(AOS_ERROR_WRAP(ErrorEnum::eFailed));
        }
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::RemoveOutdatedInstances()
{
    Error firstErr = ErrorEnum::eNone;
    auto  now      = Time::Now();

    for (auto instance = mCachedInstances.begin(); instance != mCachedInstances.end();) {
        if (now.Sub((*instance)->GetInfo().mTimestamp) >= mConfig.mServiceTTL) {
            if (auto err = (*instance)->Remove(); !err.IsNone()) {
                firstErr = err;
            }

            instance = mCachedInstances.Erase(instance);
        } else {
            instance++;
        }
    }

    return firstErr;
}

Error InstanceManager::ClearInstancesWithDeletedImages()
{
    for (auto instance = mActiveInstances.begin(); instance != mActiveInstances.end();) {
        if (!(*instance)->IsImageValid()) {
            LOG_DBG() << "Image invalid for instance: "
                      << Log::Field("instanceID", (*instance)->GetInfo().mInstanceIdent);

            if (auto err = (*instance)->Remove(); !err.IsNone()) {
                return err;
            }

            instance = mActiveInstances.Erase(instance);
        } else {
            instance++;
        }
    }

    for (auto instance = mCachedInstances.begin(); instance != mCachedInstances.end();) {
        if (!(*instance)->IsImageValid()) {
            LOG_DBG() << "Image invalid for cached instance: "
                      << Log::Field("instanceID", (*instance)->GetInfo().mInstanceIdent);

            if (auto err = (*instance)->Remove(); !err.IsNone()) {
                return err;
            }

            instance = mCachedInstances.Erase(instance);
        } else {
            instance++;
        }
    }

    return ErrorEnum::eNone;
}

RetWithError<SharedPtr<Instance>> InstanceManager::CreateInstance(const InstanceInfo& info)
{
    SharedPtr<Instance> newInstance;

    switch (info.mUpdateItemType.GetValue()) {
    case UpdateItemTypeEnum::eService:
        newInstance
            = MakeShared<ServiceInstance>(&mAllocator, info, mUIDPool, *mStorage, *mStorageState, *mImageInfoProvider);
        break;

    case UpdateItemTypeEnum::eComponent:
        newInstance = MakeShared<ComponentInstance>(&mAllocator, info, *mStorage, *mImageInfoProvider);
        break;

    default:
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotSupported)};
    }

    if (auto err = newInstance->Init(); !err.IsNone()) {
        return {{}, AOS_ERROR_WRAP(err)};
    }

    return newInstance;
}

} // namespace aos::cm::launcher
