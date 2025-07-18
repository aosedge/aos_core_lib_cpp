/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aos/cm/launcher/launcher.hpp>

#include "log.hpp"

namespace aos::cm::launcher {
Error InstanceManager::Init(const Config& config, storage::StorageItf& storage,
    imageprovider::ImageProviderItf& imageProvider, storagestate::StorageStateItf& storageState)
{
    mConfig        = config;
    mStorage       = &storage;
    mImageProvider = &imageProvider;
    mStorageState  = &storageState;

    mRunInstances.Clear();
    mErrorStatus.Clear();

    mAvailableState   = 0;
    mAvailableStorage = 0;

    return ErrorEnum::eNone;
}

Error InstanceManager::Start()
{
    if (auto err = InitUIDPool(); !err.IsNone()) {
        LOG_ERR() << "Can't init UID pool" << Log::Field(err);

        return err;
    }

    if (auto err = ClearInstancesWithDeletedServices(); !err.IsNone()) {
        LOG_ERR() << "Can't clear instances with deleted service" << Log::Field(err);

        return err;
    }

    if (auto err = RemoveOutdatedInstances(); !err.IsNone()) {
        LOG_ERR() << "Can't remove outdated instances" << Log::Field(err);

        return err;
    }

    auto func = [this](void*) {
        if (auto rmErr = RemoveOutdatedInstances(); !rmErr.IsNone()) {
            LOG_ERR() << "Can't remove outdated instances" << Log::Field(rmErr);
        }
    };

    if (auto err = mCleanInstancesTimer.Start(cRemovePeriod, func); !err.IsNone()) {
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

Error InstanceManager::UpdateInstanceCache()
{
    auto instances = MakeUnique<StaticArray<storage::InstanceInfo, cMaxNumInstances>>(&mAllocator);
    if (auto err = mStorage->GetInstances(*instances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mRunInstances.Clear();

    for (const auto& instance : *instances) {
        if (instance.mState == InstanceStateEnum::eCached) {
            continue;
        }

        if (auto err = mRunInstances.PushBack(instance); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

void InstanceManager::SetAvailableStorageStateSize(uint64_t storageSize, uint64_t stateSize)
{
    LOG_DBG() << "Available storage and state" << Log::Field("availableStorage", storageSize)
              << Log::Field("availableState", stateSize);

    mAvailableStorage = storageSize;
    mAvailableState   = stateSize;
}

void InstanceManager::OnServiceRemoved(const String& serviceID)
{
    (void)serviceID;
    // TODO: Clarify what to do
}

Error InstanceManager::GetInstanceCheckSum(const InstanceIdent& instanceID, String& checkSum)
{
    return mStorageState->GetInstanceCheckSum(instanceID, checkSum);
}

Error InstanceManager::CacheInstance(const storage::InstanceInfo& instance)
{
    LOG_DBG() << "Cache instance" << Log::Field("instanceID", instance.mInstanceID);

    auto info = MakeUnique<storage::InstanceInfo>(&mAllocator);

    *info         = instance;
    info->mState  = InstanceStateEnum::eCached;
    info->mNodeID = "";

    if (auto err = mStorage->UpdateInstance(*info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorageState->Cleanup(instance.mInstanceID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void InstanceManager::SetInstanceError(const InstanceIdent& id, const String& serviceVersion, const Error& err)
{
    auto it
        = mErrorStatus.FindIf([&id](const nodemanager::InstanceStatus& status) { return id == status.mInstanceIdent; });

    if (it == mErrorStatus.end()) {
        if (auto emplaceErr = mErrorStatus.EmplaceBack(); !emplaceErr.IsNone()) {
            LOG_ERR() << "Failed to set instance error status" << AOS_ERROR_WRAP(emplaceErr);

            return;
        }

        it = mErrorStatus.end() - 1;
    }

    it->mInstanceIdent  = id;
    it->mServiceVersion = serviceVersion;
    it->mRunState       = InstanceRunStateEnum::eFailed;

    if (!err.IsNone()) {
        LOG_ERR() << "Schedule instance error" << Log::Field(err);

        it->mError = err;
    }
}

Error InstanceManager::GetInstanceInfo(const InstanceIdent& id, storage::InstanceInfo& info)
{
    for (const auto& instance : mRunInstances) {
        if (instance.mInstanceID == id) {
            info = instance;

            return ErrorEnum::eNone;
        }
    }

    return ErrorEnum::eNotFound;
}

Error InstanceManager::SetupInstance(const RunInstanceRequest& request, NodeHandler& nodeHandler,
    const imageprovider::ServiceInfo& serviceInfo, bool rebalancing, aos::InstanceInfo& info)
{
    info.mInstanceIdent = request.mInstanceID;
    info.mPriority      = request.mPriority;

    auto storedInstance = MakeUnique<storage::InstanceInfo>(&mAllocator);
    if (auto err = mStorage->GetInstance(request.mInstanceID, *storedInstance); !err.IsNone()) {
        if (!err.Is(ErrorEnum::eNotFound)) {
            return AOS_ERROR_WRAP(err);
        }

        auto [uid, uidErr] = mUIDPool.Acquire();
        if (!uidErr.IsNone()) {
            return AOS_ERROR_WRAP(uidErr);
        }

        storedInstance->mInstanceID = request.mInstanceID;
        storedInstance->mNodeID     = nodeHandler.GetInfo().mNodeID;
        storedInstance->mUID        = uid;
        storedInstance->mTimestamp  = Time::Now();

        if (auto addErr = mStorage->AddInstance(*storedInstance); !addErr.IsNone()) {
            LOG_ERR() << "Can't add instance" << Log::Field(AOS_ERROR_WRAP(addErr));
        }
    } else {
        if (rebalancing) {
            storedInstance->mPrevNodeID = storedInstance->mNodeID;
        } else {
            storedInstance->mPrevNodeID = "";
        }

        storedInstance->mNodeID    = nodeHandler.GetInfo().mNodeID;
        storedInstance->mTimestamp = Time::Now();
        storedInstance->mState     = InstanceStateEnum::eActive;

        if (auto updateErr = mStorage->UpdateInstance(*storedInstance); !updateErr.IsNone()) {
            LOG_ERR() << "Can't update instance" << Log::Field(updateErr);
        }
    }

    LOG_DBG() << "Setup instance" << Log::Field("instanceID", request.mInstanceID)
              << Log::Field("curNodeID", storedInstance->mNodeID)
              << Log::Field("prevNodeID", storedInstance->mPrevNodeID);

    info.mUID = storedInstance->mUID;

    auto reqState   = nodeHandler.GetReqStateSize(serviceInfo.mConfig);
    auto reqStorage = nodeHandler.GetReqStorageSize(serviceInfo.mConfig);

    LOG_DBG() << "Requested storage and state" << Log::Field("instanceID", request.mInstanceID)
              << Log::Field("reqStorage", reqStorage) << Log::Field("reqState", reqState);

    if (auto err = SetupInstanceStateStorage(serviceInfo, reqState, reqStorage, info); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

bool InstanceManager::IsInstanceScheduled(const InstanceIdent& id) const
{
    bool exist = mRunInstances.ExistIf([&id](const storage::InstanceInfo& info) { return id == info.mInstanceID; });

    if (exist) {
        return true;
    }

    exist = mErrorStatus.ExistIf(
        [&id](const nodemanager::InstanceStatus& status) { return id == status.mInstanceIdent; });

    return exist;
}

const Array<storage::InstanceInfo> InstanceManager::GetRunningInstances() const
{
    return mRunInstances;
}

const Array<nodemanager::InstanceStatus> InstanceManager::GetErrorStatuses() const
{
    return mErrorStatus;
}

Error InstanceManager::InitUIDPool()
{
    auto allUIDsValid = [](size_t id) {
        (void)id;
        return true;
    };

    if (auto err = mUIDPool.Init(allUIDsValid); !err.IsNone()) {
        return err;
    }

    auto instances = MakeUnique<StaticArray<storage::InstanceInfo, cMaxNumInstances>>(&mAllocator);
    if (auto err = mStorage->GetInstances(*instances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& instance : *instances) {
        if (auto err = mUIDPool.TryAcquire(instance.mUID); !err.IsNone()) {
            LOG_WRN() << "Can't add UID to pool" << Log::Field(err);
        }
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::ClearInstancesWithDeletedServices()
{
    auto instances = MakeUnique<StaticArray<storage::InstanceInfo, cMaxNumInstances>>(&mAllocator);
    if (auto err = mStorage->GetInstances(*instances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto serviceInfo = MakeUnique<imageprovider::ServiceInfo>(&mAllocator);

    for (const auto& instance : *instances) {
        if (auto err = mImageProvider->GetServiceInfo(instance.mInstanceID.mServiceID, *serviceInfo); err.IsNone()) {
            continue;
        }

        if (auto err = RemoveInstance(instance); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::RemoveOutdatedInstances()
{
    auto instances = MakeUnique<StaticArray<storage::InstanceInfo, cMaxNumInstances>>(&mAllocator);
    if (auto err = mStorage->GetInstances(*instances); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Error firstErr = ErrorEnum::eNone;
    for (const auto& instance : *instances) {
        if (instance.mState != InstanceStateEnum::eCached
            || Time::Now().Sub(instance.mTimestamp) < mConfig.mServiceTTL) {
            continue;
        }

        if (auto err = RemoveInstance(instance); !err.IsNone()) {
            LOG_ERR() << "Can't remove instance" << Log::Field(err);

            if (firstErr.IsNone()) {
                firstErr = err;
            }
        }
    }

    return firstErr;
}

Error InstanceManager::RemoveInstance(const storage::InstanceInfo& instance)
{
    if (auto err = mStorageState->Remove(instance.mInstanceID); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorage->RemoveInstance(instance.mInstanceID); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mUIDPool.Release(instance.mUID); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::SetupInstanceStateStorage(
    const imageprovider::ServiceInfo& serviceInfo, uint64_t reqState, uint64_t reqStorage, InstanceInfo& info)
{
    storagestate::SetupParams params;

    params.mInstanceIdent = info.mInstanceIdent;
    params.mUID           = info.mUID;
    params.mGID           = serviceInfo.mGID;

    if (serviceInfo.mConfig.mQuotas.mStateLimit.HasValue()) {
        params.mStateQuota = *serviceInfo.mConfig.mQuotas.mStateLimit;
    }

    if (serviceInfo.mConfig.mQuotas.mStorageLimit.HasValue()) {
        params.mStorageQuota = *serviceInfo.mConfig.mQuotas.mStorageLimit;
    }

    if (reqStorage > mAvailableStorage && !serviceInfo.mConfig.mSkipResourceLimits) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "not enough storage space"));
    }

    if (reqState > mAvailableState && !serviceInfo.mConfig.mSkipResourceLimits) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "not enough state space"));
    }

    if (auto err = mStorageState->Setup(params, info.mStoragePath, info.mStatePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!serviceInfo.mConfig.mSkipResourceLimits) {
        mAvailableState -= reqState;
        mAvailableStorage -= reqStorage;
    }

    LOG_DBG() << "Remaining storage and state" << Log::Field("remainingState", mAvailableState)
              << Log::Field("remainingStorage", mAvailableStorage);

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
