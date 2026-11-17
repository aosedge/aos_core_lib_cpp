/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "runtimemanager.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error RuntimeManager::Init(AllocatorItf& allocator, const Array<RuntimeItf*>& runtimes)
{
    mAllocator = &allocator;

    for (auto* runtime : runtimes) {
        if (auto err = mRuntimes.Set(runtime, ""); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error RuntimeManager::Start()
{
    for (auto& it : mRuntimes) {
        if (auto err = it.mFirst->Start(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (auto& it : mRuntimes) {
        RuntimeInfo runtimeInfo;

        if (auto err = it.mFirst->GetRuntimeInfo(runtimeInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = it.mSecond.Assign(runtimeInfo.mRuntimeID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        LOG_INF() << "Registered runtime" << Log::Field("runtimeID", runtimeInfo.mRuntimeID)
                  << Log::Field("type", runtimeInfo.mRuntimeType);
    }

    return ErrorEnum::eNone;
}

Error RuntimeManager::Stop()
{
    Error stopErr;

    for (auto& it : mRuntimes) {
        if (auto err = it.mFirst->Stop(); !err.IsNone() && stopErr.IsNone()) {
            stopErr = AOS_ERROR_WRAP(err);
        }
    }

    return stopErr;
}

RuntimeItf* RuntimeManager::Find(const String& runtimeID)
{
    if (auto it = mRuntimes.FindIf([&runtimeID](const auto& it) { return it.mSecond == runtimeID; });
        it != mRuntimes.end()) {
        return it->mFirst;
    }

    return nullptr;
}

Error RuntimeManager::GetRuntimesInfos(Array<RuntimeInfo>& runtimes) const
{
    for (const auto& runtime : mRuntimes) {
        if (auto err = runtimes.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = runtime.mFirst->GetRuntimeInfo(runtimes.Back()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error RuntimeManager::InitInstances(const Array<InstanceInfo>& instances)
{
    LOG_DBG() << "Init instances" << Log::Field("numInstances", instances.Size());

    for (auto& it : mRuntimes) {
        auto runtimeInstances = MakeUnique<InstanceInfoArray>(mAllocator);
        if (!runtimeInstances) {
            return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
        }

        for (const auto& instanceInfo : instances) {
            if (instanceInfo.mRuntimeID != it.mSecond) {
                continue;
            }

            if (auto err = runtimeInstances->PushBack(instanceInfo); !err.IsNone()) {
                LOG_ERR() << "Failed to add instance to runtime init list" << Log::Field("instance", instanceInfo)
                          << Log::Field(AOS_ERROR_WRAP(err));

                break;
            }
        }

        if (auto err = it.mFirst->InitInstances(*runtimeInstances); !err.IsNone()) {
            LOG_ERR() << "Failed to init instances" << Log::Field("runtimeID", it.mSecond)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
