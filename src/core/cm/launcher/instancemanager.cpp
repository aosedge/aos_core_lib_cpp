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

Error InstanceManager::Init(const Config& config, imagemanager::ItemInfoProviderItf& itemInfoProvider,
    storagestate::StorageStateItf& storageState, oci::OCISpecItf& ociSpec, IdentifierPoolValidator gidValidator,
    IdentifierPoolValidator uidValidator, StorageItf& storage, NetworkManager& networkManager)
{
    mConfig         = config;
    mStorage        = &storage;
    mNetworkManager = &networkManager;

    mImageInfoProvider.Init(itemInfoProvider, ociSpec);
    mStorageState.Init(storageState);

    if (auto err = mUIDPool.Init(uidValidator); !err.IsNone()) {
        return err;
    }

    if (auto err = mGIDPool.Init(gidValidator); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::Start()
{
    if (auto err = mStorageState.Start(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
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

    if (auto err = mGIDPool.Clear(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorageState.Stop(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mActiveInstances.Clear();
    mScheduledInstances.Clear();
    mCachedInstances.Clear();
    mPreinstalledComponents.Clear();
    mRunningInstances.Clear();

    return ErrorEnum::eNone;
}

Error InstanceManager::PrepareForBalancing()
{
    if (auto err = mStorageState.PrepareForBalancing(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mScheduledInstances.Clear();

    return ErrorEnum::eNone;
}

Array<SharedPtr<Instance>>& InstanceManager::GetActiveInstances()
{
    return mActiveInstances;
}

Array<SharedPtr<Instance>>& InstanceManager::GetCachedInstances()
{
    return mCachedInstances;
}

Array<SharedPtr<Instance>>& InstanceManager::GetScheduledInstances()
{
    return mScheduledInstances;
}

Array<InstanceStatus>& InstanceManager::GetPreinstalledComponents()
{
    return mPreinstalledComponents;
}

Array<InstanceStatus>& InstanceManager::GetRunningInstances()
{
    return mRunningInstances;
}

Error InstanceManager::UpdateStatus(const InstanceStatus& status)
{
    if (status.mPreinstalled) {
        auto preinstalledComponent = FindPreinstalledComponent(status);
        if (preinstalledComponent == nullptr) {
            if (auto err = mPreinstalledComponents.EmplaceBack(status); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            return ErrorEnum::eNone;
        }

        *preinstalledComponent = status;

        return ErrorEnum::eNone;
    }

    auto instance = FindActiveInstance(status);
    if (!instance) {
        // Ignore inactive instance, SM sometimes sends inactive status for stopped instances.
        if (status.mState == aos::InstanceStateEnum::eInactive) {
            return ErrorEnum::eNone;
        }

        // Not expected instance received from SM.
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return instance->UpdateStatus(status);
}

RetWithError<SharedPtr<Instance>> InstanceManager::CreateInstance(
    const InstanceIdent& id, const RunInstanceRequest& request)
{
    if (auto instance = FindReadyInstance(id); instance) {
        return {instance, ErrorEnum::eNone};
    }

    auto instanceInfo = MakeUnique<InstanceInfo>(&mAllocator);
    CreateInfo(id, request, *instanceInfo);

    if (auto err = mStorage->AddInstance(*instanceInfo); !err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    return CreateInstance(*instanceInfo);
}

Error InstanceManager::SubmitScheduledInstances()
{
    for (auto& instance : mActiveInstances) {
        auto isStashed = mScheduledInstances.ContainsIf(
            [&instance](const SharedPtr<Instance>& item) { return instance.Get() == item.Get(); });

        // Cache deleted instances
        if (!isStashed) {
            if (auto err = instance->Cache(); !err.IsNone()) {
                const auto& id = instance->GetInfo().mInstanceIdent;

                LOG_ERR() << "Cache instance failed" << Log::Field("instanceID", id) << AOS_ERROR_WRAP(err);
            }

            mCachedInstances.PushBack(instance);
        }
    }

    mActiveInstances = mScheduledInstances;
    mCachedInstances.RemoveIf([this](const SharedPtr<Instance>& instance) {
        return mActiveInstances.ContainsIf(
            [instance](const SharedPtr<Instance>& item) { return instance.Get() == item.Get(); });
    });

    mScheduledInstances.Clear();

    return ErrorEnum::eNone;
}

Error InstanceManager::DisableInstance(SharedPtr<Instance>& instance)
{
    if (auto err = instance->Cache(true); !err.IsNone()) {
        const auto& id = instance->GetInfo().mInstanceIdent;

        LOG_ERR() << "Disable instance failed" << Log::Field("instanceID", id) << AOS_ERROR_WRAP(err);
    }

    mCachedInstances.PushBack(instance);

    mScheduledInstances.Remove(instance);
    mActiveInstances.Remove(instance);

    return ErrorEnum::eNone;
}

SharedPtr<Instance> InstanceManager::FindActiveInstance(const InstanceIdent& id)
{
    auto it = mActiveInstances.FindIf(
        [&id](SharedPtr<Instance>& instance) { return instance->GetInfo().mInstanceIdent == id; });

    return it != mActiveInstances.end() ? *it : SharedPtr<Instance>();
}

SharedPtr<Instance> InstanceManager::FindScheduledInstance(const InstanceIdent& id)
{
    auto it = mScheduledInstances.FindIf(
        [&id](SharedPtr<Instance>& instance) { return instance->GetInfo().mInstanceIdent == id; });

    return it != mScheduledInstances.end() ? *it : SharedPtr<Instance>();
}

SharedPtr<Instance> InstanceManager::FindCachedInstance(const InstanceIdent& id)
{
    auto it = mCachedInstances.FindIf(
        [&id](SharedPtr<Instance>& instance) { return instance->GetInfo().mInstanceIdent == id; });

    return it != mCachedInstances.end() ? *it : SharedPtr<Instance>();
}

InstanceStatus* InstanceManager::FindPreinstalledComponent(const InstanceIdent& id)
{
    auto component = mPreinstalledComponents.FindIf(
        [&id](const InstanceStatus& status) { return static_cast<const InstanceIdent&>(status) == id; });

    return component != mPreinstalledComponents.end() ? component : nullptr;
}

void InstanceManager::UpdateMonitoringData(const Array<monitoring::InstanceMonitoringData>& monitoringData)
{
    for (const auto& instanceData : monitoringData) {
        auto instance = FindActiveInstance(instanceData.mInstanceIdent);
        if (instance) {
            instance->UpdateMonitoringData(instanceData.mMonitoringData);
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

    LOG_DBG() << "Load active instances" << Log::Field("instances", instances->Size());

    for (const auto& instance : *instances) {
        if (auto err = LoadInstanceFromStorage(instance); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = LoadInstanceStatuses(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::LoadInstanceFromStorage(const InstanceInfo& info)
{
    auto [instance, createErr] = CreateInstance(info);
    if (!createErr.IsNone()) {
        return createErr;
    }

    if (info.mState == InstanceStateEnum::eActive) {
        LOG_DBG() << "Load active instance" << Log::Field("instanceID", instance->GetInfo().mInstanceIdent)
                  << Log::Field("nodeID", instance->GetStatus().mNodeID);

        if (auto err = mActiveInstances.EmplaceBack(instance); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else {
        LOG_DBG() << "Load cached instance" << Log::Field("instanceID", instance->GetInfo().mInstanceIdent)
                  << Log::Field("nodeID", instance->GetStatus().mNodeID);

        if (auto err = mCachedInstances.EmplaceBack(instance); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::LoadInstanceStatuses()
{
    for (auto& instance : mActiveInstances) {
        if (!instance->GetInfo().mNodeID.IsEmpty()) {
            if (auto err = mRunningInstances.EmplaceBack(instance->GetStatus()); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::SetExpiredStatus()
{
    for (auto& instance : mActiveInstances) {
        if (instance->GetStatus().mState == aos::InstanceStateEnum::eActivating) {
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
        if (now.Sub((*instance)->GetInfo().mTimestamp) >= mConfig.mInstanceTTL) {
            if (auto err = (*instance)->Remove(); !err.IsNone() && firstErr.IsNone()) {
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
    Error firstErr = ErrorEnum::eNone;

    for (auto instance = mActiveInstances.begin(); instance != mActiveInstances.end();) {
        if (!(*instance)->IsImageValid()) {
            LOG_DBG() << "Image invalid for running instance: "
                      << Log::Field("instanceID", (*instance)->GetInfo().mInstanceIdent);

            if (auto err = (*instance)->Remove(); !err.IsNone() && firstErr.IsNone()) {
                firstErr = err;
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

            if (auto err = (*instance)->Remove(); !err.IsNone() && firstErr.IsNone()) {
                firstErr = err;
            }

            instance = mCachedInstances.Erase(instance);
        } else {
            instance++;
        }
    }

    return firstErr;
}

RetWithError<SharedPtr<Instance>> InstanceManager::CreateInstance(const InstanceInfo& info)
{
    SharedPtr<Instance> newInstance;

    switch (info.mInstanceIdent.mType.GetValue()) {
    case UpdateItemTypeEnum::eService:
        newInstance = MakeShared<ServiceInstance>(&mAllocator, info, mUIDPool, mGIDPool, *mStorage, mStorageState,
            mImageInfoProvider, *mNetworkManager, mInstanceAllocator);
        break;

    case UpdateItemTypeEnum::eComponent:
        newInstance
            = MakeShared<ComponentInstance>(&mAllocator, info, *mStorage, mImageInfoProvider, mInstanceAllocator);
        break;

    default:
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotSupported)};
    }

    if (auto err = newInstance->Init(); !err.IsNone()) {
        return {{}, AOS_ERROR_WRAP(err)};
    }

    return newInstance;
}

RetWithError<bool> InstanceManager::SetSubjects(const Array<StaticString<cIDLen>>& subjects)
{
    if (auto err = mSubjects.Assign(subjects); !err.IsNone()) {
        return {false, AOS_ERROR_WRAP(err)};
    }

    for (const auto& instance : mActiveInstances) {
        if (!IsSubjectEnabled(*instance)) {
            return {true, ErrorEnum::eNone};
        }
    }

    for (const auto& instance : mCachedInstances) {
        if (IsSubjectEnabled(*instance) && instance->GetInfo().mState == InstanceStateEnum::eDisabled) {
            return {true, ErrorEnum::eNone};
        }
    }

    return {false, ErrorEnum::eNone};
}

bool InstanceManager::IsSubjectEnabled(const Instance& instance)
{
    return !instance.GetInfo().mIsUnitSubject || mSubjects.Contains(instance.GetInfo().mInstanceIdent.mSubjectID);
}

bool InstanceManager::IsScheduled(const InstanceIdent& id)
{
    return FindScheduledInstance(id).Get() != nullptr;
}

Error InstanceManager::UpdateRunningInstances(const String& nodeID, const Array<InstanceStatus>& statuses)
{
    mRunningInstances.RemoveIf([&nodeID](const InstanceStatus& status) { return status.mNodeID == nodeID; });
    mPreinstalledComponents.RemoveIf([&nodeID](const InstanceStatus& status) { return status.mNodeID == nodeID; });

    for (const auto& status : statuses) {
        if (status.mNodeID == nodeID) {
            if (status.mPreinstalled) {
                if (auto err = mPreinstalledComponents.EmplaceBack(status); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }
            } else {
                if (auto err = mRunningInstances.EmplaceBack(status); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }
            }
        }
    }

    Error firstErr = ErrorEnum::eNone;

    for (const auto& status : statuses) {
        if (auto err = UpdateStatus(status); !err.IsNone() && firstErr.IsNone()) {
            firstErr = err;
        }
    }

    return firstErr;
}

Error InstanceManager::ScheduleInstance(SharedPtr<Instance>& instance, NodeItf& node, const String& runtimeID)
{
    if (auto err = instance->Schedule(node, runtimeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mScheduledInstances.EmplaceBack(instance); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error InstanceManager::ScheduleInstance(SharedPtr<Instance>& instance, const Error& error)
{
    instance->SetError(error);

    if (auto err = mScheduledInstances.EmplaceBack(instance); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

SharedPtr<Instance> InstanceManager::FindReadyInstance(const InstanceIdent& id)
{
    auto instance = FindScheduledInstance(id);
    if (instance) {
        return instance;
    }

    instance = FindActiveInstance(id);
    if (instance) {
        return instance;
    }

    instance = FindCachedInstance(id);
    if (instance) {
        return instance;
    }

    return nullptr;
}

void InstanceManager::CreateInfo(const InstanceIdent& id, const RunInstanceRequest& request, InstanceInfo& info)
{
    info.mInstanceIdent  = id;
    info.mManifestDigest = "";
    info.mNodeID         = "";
    info.mPrevNodeID     = "";
    info.mRuntimeID      = "";
    info.mUID            = 0;
    info.mGID            = 0;
    info.mTimestamp      = Time::Now();
    info.mState          = InstanceStateEnum::eActive;
    info.mIsUnitSubject  = request.mSubjectInfo.mIsUnitSubject;
    info.mVersion        = request.mVersion;
    info.mOwnerID        = request.mOwnerID;
    info.mSubjectType    = request.mSubjectInfo.mSubjectType;
    info.mLabels         = request.mLabels;
    info.mPriority       = request.mPriority;
}

} // namespace aos::cm::launcher
