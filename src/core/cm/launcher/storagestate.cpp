/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storagestate.hpp"

#include <core/common/tools/logger.hpp>

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void StorageState::Init(storagestate::StorageStateItf& storageState)
{
    mStorageStateManager = &storageState;
}

Error StorageState::Start()
{
    return PrepareForBalancing();
}

Error StorageState::Stop()
{
    mAvailableStorage.Reset();
    mAvailableState.Reset();

    return ErrorEnum::eNone;
}

Error StorageState::PrepareForBalancing()
{
    mAvailableState.Reset();
    mAvailableStorage.Reset();

    if (mStorageStateManager->IsSamePartition()) {
        mAvailableState = mAvailableStorage = MakeShared<size_t>(&mAllocator, 0);
    } else {
        mAvailableState   = MakeShared<size_t>(&mAllocator, 0);
        mAvailableStorage = MakeShared<size_t>(&mAllocator, 0);
    }

    const auto& [stateSize, stateErr] = mStorageStateManager->GetTotalStateSize();
    if (!stateErr.IsNone()) {
        return AOS_ERROR_WRAP(stateErr);
    }

    *mAvailableState = stateSize;

    const auto& [storageSize, storageErr] = mStorageStateManager->GetTotalStorageSize();
    if (!storageErr.IsNone()) {
        return AOS_ERROR_WRAP(storageErr);
    }

    *mAvailableStorage = storageSize;

    return ErrorEnum::eNone;
}

Error StorageState::Cleanup(const InstanceIdent& instanceIdent)
{
    return mStorageStateManager->Cleanup(instanceIdent);
}

Error StorageState::Remove(const InstanceIdent& instanceIdent)
{
    return mStorageStateManager->Remove(instanceIdent);
}

Error StorageState::SetupStateStorage(const InstanceIdent& instanceIdent, const storagestate::SetupParams& setupParams,
    size_t requestedStorageSize, size_t requestedStateSize, String& storagePath, String& statePath)
{
    // Check available storage size
    if (requestedStorageSize > *mAvailableStorage) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "not enough storage space"));
    }

    auto availableStorage = *mAvailableStorage;
    *mAvailableStorage    = *mAvailableStorage - requestedStorageSize;
    auto restoreStorageSize
        = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { *mAvailableStorage = availableStorage; });

    // Check available state size
    if (requestedStateSize > *mAvailableState) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNoMemory, "not enough state space"));
    }

    auto availableState   = *mAvailableState;
    *mAvailableState      = *mAvailableState - requestedStateSize;
    auto restoreStateSize = DeferRelease(reinterpret_cast<int*>(1), [&](int*) { *mAvailableState = availableState; });

    // Setup storage and state
    if (auto err = mStorageStateManager->Setup(instanceIdent, setupParams, storagePath, statePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    restoreStorageSize.Release();
    restoreStateSize.Release();

    LOG_DBG() << "Available storage and state" << Log::Field("state", *mAvailableState)
              << Log::Field("storage", *mAvailableStorage);

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
