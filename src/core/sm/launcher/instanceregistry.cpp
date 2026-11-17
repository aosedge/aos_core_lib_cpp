/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/utils.hpp>

#include "instanceregistry.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void InstanceRegistry::Init(InstanceIDProviderItf& instanceIDProvider)
{
    mInstanceIDProvider = &instanceIDProvider;
}

RetWithError<InstanceData*> InstanceRegistry::Add(const InstanceInfo& instanceInfo)
{
    if (auto err = mInstances.EmplaceBack(); !err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    auto instance = &mInstances.Back();

    instance->mInfo = instanceInfo;
    static_cast<InstanceIdent&>(instance->mStatus)
        = instanceInfo; // NOSONAR cpp:S5912 - intentional copy of InstanceIdent base only
    instance->mStatus.mVersion   = instanceInfo.mVersion;
    instance->mStatus.mRuntimeID = instanceInfo.mRuntimeID;
    instance->mStatus.mState     = InstanceStateEnum::eInactive;

    if (auto err = mInstanceIDProvider->GetInstanceID(instanceInfo, instance->mInstanceID); !err.IsNone()) {
        (void)mInstances.Erase(instance);

        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    return instance;
}

void InstanceRegistry::Remove(const InstanceIdent& instanceIdent)
{
    (void)mInstances.RemoveIf([&instanceIdent](const auto& instance) {
        return static_cast<const InstanceIdent&>(instance.mInfo) == instanceIdent;
    });
}

InstanceData* InstanceRegistry::Find(const InstanceIdent& instanceIdent)
{
    if (auto it = mInstances.FindIf([&instanceIdent](const auto& instance) {
            return static_cast<const InstanceIdent&>(instance.mInfo) == instanceIdent;
        });
        it != mInstances.end()) {
        return it;
    }

    return nullptr;
}

const InstanceData* InstanceRegistry::Find(const InstanceIdent& instanceIdent) const
{
    if (auto it = mInstances.FindIf([&instanceIdent](const auto& instance) {
            return static_cast<const InstanceIdent&>(instance.mInfo) == instanceIdent;
        });
        it != mInstances.end()) {
        return it;
    }

    return nullptr;
}

InstanceData* InstanceRegistry::FindByID(const String& instanceID)
{
    if (auto it = mInstances.FindIf([&instanceID](const auto& instance) { return instance.mInstanceID == instanceID; });
        it != mInstances.end()) {
        return it;
    }

    return nullptr;
}

Error InstanceRegistry::GetStatuses(Array<InstanceStatus>& statuses) const
{
    statuses.Clear();

    for (const auto& instance : mInstances) {
        if (auto err = statuses.EmplaceBack(instance.mStatus); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Optional<Duration> InstanceRegistry::GetMinOfflineTTL() const
{
    Optional<Duration> earliest;

    for (const auto& instance : mInstances) {
        if (!instance.mOfflineTTL
            || (instance.mStatus.mState != InstanceStateEnum::eActive
                && instance.mStatus.mState != InstanceStateEnum::eActivating)) {
            continue;
        }

        if (!earliest.HasValue()) {
            earliest = instance.mOfflineTTL;

            continue;
        }

        earliest = Min(*earliest, instance.mOfflineTTL);
    }

    return earliest;
}

} // namespace aos::sm::launcher
