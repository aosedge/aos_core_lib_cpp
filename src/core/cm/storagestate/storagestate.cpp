/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/stat.h>
#include <unistd.h>

#include <core/common/tools/logger.hpp>

#include "storagestate.hpp"

namespace aos::cm::storagestate {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

Error ToRelativePath(const String& base, const String& full, String& result)
{
    if (full.Size() < base.Size()) {
        return ErrorEnum::eInvalidArgument;
    }

    if (auto findResult = full.FindSubstr(0, base); !findResult.mError.IsNone() || findResult.mValue != 0) {
        return ErrorEnum::eInvalidArgument;
    }

    if (auto err = result.Assign(full.begin() + base.Size()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    result.LeftTrim("/");

    return ErrorEnum::eNone;
}

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error StorageState::Init(const Config& config, StorageItf& storage, SenderItf& sender, fs::FSPlatformItf& fsPlatform,
    fs::FSWatcherItf& fsWatcher, crypto::HasherItf& hasher)
{
    LOG_INF() << "Initialize storage state";

    mConfig        = config;
    mStorage       = &storage;
    mMessageSender = &sender;
    mFSPlatform    = &fsPlatform;
    mFSWatcher     = &fsWatcher;
    mHasher        = &hasher;

    if (auto err = fs::MakeDirAll(mConfig.mStateDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::MakeDirAll(mConfig.mStorageDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Error                      err;
    StaticString<cFilePathLen> storageMountPoint;
    StaticString<cFilePathLen> stateMountPoint;

    Tie(storageMountPoint, err) = mFSPlatform->GetMountPoint(mConfig.mStorageDir);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Tie(stateMountPoint, err) = mFSPlatform->GetMountPoint(mConfig.mStateDir);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mStateAndStorageOnSamePartition = (storageMountPoint == stateMountPoint);

    return ErrorEnum::eNone;
}

Error StorageState::Start()
{
    LockGuard lock {mMutex};

    LOG_INF() << "Start storage state";

    if (auto err = InitStateWatching(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return mThreadPool.Run();
}

Error StorageState::Stop()
{
    LockGuard lock {mMutex};

    LOG_INF() << "Stop storage state";

    while (!mStates.IsEmpty()) {
        if (auto err = StopStateWatching(mStates.Front().mInstanceIdent);
            !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
            LOG_WRN() << "Failed to stop state watching" << Log::Field(err);
        }
    }

    return mThreadPool.Shutdown();
}

Error StorageState::UpdateState(const aos::UpdateState& state)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Update state" << Log::Field("instanceIdent", static_cast<const InstanceIdent&>(state))
              << Log::Field("size", state.mState.Size());

    auto it = mStates.FindIf([&state](const auto& item) { return item.mInstanceIdent == state; });
    if (it == mStates.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (state.mState.Size() > it->mQuota) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "update state exceeds quota"));
    }

    if (auto err = ValidateChecksum(state.mState, state.mChecksum); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto storageStateInfo = MakeUnique<InstanceInfo>(&mAllocator);

    if (auto err = mStorage->GetStorageStateInfo(state, *storageStateInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = storageStateInfo->mStateChecksum.Assign(state.mChecksum); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorage->UpdateStorageStateInfo(*storageStateInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::WriteStringToFile(it->mFilePath, state.mState, S_IRUSR | S_IWUSR); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = it->mChecksum.Assign(state.mChecksum); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StorageState::AcceptState(const StateAcceptance& state)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "State acceptance" << Log::Field("instanceIdent", static_cast<const InstanceIdent&>(state))
              << Log::Field("reason", state.mReason);

    auto it = mStates.FindIf(([&state](const auto& item) { return item.mInstanceIdent == state; }));
    if (it == mStates.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (it->mChecksum != state.mChecksum) {
        LOG_DBG() << "State checksum mismatch";

        return AOS_ERROR_WRAP(ErrorEnum::eInvalidChecksum);
    }

    if (state.mResult != StateResultEnum::eAccepted) {
        StateRequest request;

        static_cast<InstanceIdent&>(request) = static_cast<const InstanceIdent&>(state);
        request.mDefault                     = false;

        return mMessageSender->SendStateRequest(request);
    }

    auto storageStateInfo = MakeUnique<InstanceInfo>(&mAllocator);

    if (auto err = mStorage->GetStorageStateInfo(state, *storageStateInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = storageStateInfo->mStateChecksum.Assign(it->mChecksum); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorage->UpdateStorageStateInfo(*storageStateInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StorageState::Setup(
    const InstanceIdent& instanceIdent, const SetupParams& setupParams, String& storagePath, String& statePath)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Setup storage and state" << setupParams;

    auto storageData = MakeUnique<InstanceInfo>(&mAllocator);

    auto err = mStorage->GetStorageStateInfo(instanceIdent, *storageData);
    if (err.Is(ErrorEnum::eNotFound)) {
        storageData = MakeUnique<InstanceInfo>(&mAllocator);

        storageData->mInstanceIdent = instanceIdent;

        err = mStorage->AddStorageStateInfo(*storageData);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    } else if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = PrepareStorage(instanceIdent, setupParams, storagePath);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = StopStateWatching(instanceIdent);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        LOG_WRN() << "Failed to stop state watching" << Log::Field("instanceIdent", instanceIdent) << Log::Field(err);
    }

    if (!QuotasAreEqual(*storageData, setupParams)) {
        err = SetQuotas(setupParams);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        storageData->mStorageQuota = setupParams.mStorageQuota;
        storageData->mStateQuota   = setupParams.mStateQuota;

        err = mStorage->UpdateStorageStateInfo(*storageData);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    err = PrepareState(instanceIdent, setupParams, storageData->mStateChecksum, statePath);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StorageState::Cleanup(const InstanceIdent& instanceIdent)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Clean storage and state" << Log::Field("instanceIdent", instanceIdent);

    return StopStateWatching(instanceIdent);
}

Error StorageState::Remove(const InstanceIdent& instanceIdent)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Remove storage and state" << Log::Field("instanceIdent", instanceIdent);

    if (auto err = StopStateWatching(instanceIdent); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = RemoveFromSystem(instanceIdent); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StorageState::GetInstanceCheckSum(const InstanceIdent& instanceIdent, Array<uint8_t>& checkSum)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get instance checksum" << Log::Field("instanceIdent", instanceIdent);

    auto it = mStates.FindIf([&instanceIdent](const auto& item) { return item.mInstanceIdent == instanceIdent; });
    if (it == mStates.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return checkSum.Assign(it->mChecksum);
}

RetWithError<size_t> StorageState::GetTotalStateSize() const
{
    return mFSPlatform->GetTotalSize(mConfig.mStateDir);
}

RetWithError<size_t> StorageState::GetTotalStorageSize() const
{
    return mFSPlatform->GetTotalSize(mConfig.mStorageDir);
}

bool StorageState::IsSamePartition() const
{
    return mStateAndStorageOnSamePartition;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void StorageState::OnFSEvent(const String& path, const Array<fs::FSEvent>& events)
{
    (void)events;

    StaticString<cFilePathLen> statePath = path;

    if (auto err = mThreadPool.AddTask([this, statePath](void*) {
            LockGuard lock {mMutex};

            LOG_DBG() << "Handle file system event" << Log::Field("path", statePath);

            auto it = mStates.FindIf([&statePath](const auto& state) { return state.mFilePath == statePath; });
            if (it == mStates.end()) {
                LOG_WRN() << "Error processing state change" << Log::Field("path", statePath)
                          << Log::Field(ErrorEnum::eNotFound);

                return;
            }

            if (auto err = SendNewStateIfFileChanged(*it); !err.IsNone()) {
                LOG_ERR() << "Failed notifying state change" << Log::Field("path", statePath) << Log::Field(err);
            }
        });
        !err.IsNone()) {
        LOG_ERR() << "Failed handling file system event" << Log::Field("path", path) << Log::Field(err);
    }
}

Error StorageState::InitStateWatching()
{
    LOG_DBG() << "Initialize state watching";

    auto infos = MakeUnique<InstanceInfoArray>(&mAllocator);

    if (auto err = mStorage->GetAllStorageStateInfo(*infos); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<crypto::cSHA2DigestSize> checksum;

    for (const auto& info : *infos) {
        if (info.mStateQuota == 0) {
            continue;
        }

        if (auto err = StartStateWatching(info.mInstanceIdent, GetStateFilePath(info.mInstanceIdent), info.mStateQuota);
            !err.IsNone()) {
            LOG_ERR() << "Can't setup state watching" << Log::Field("instanceID", info.mInstanceIdent)
                      << Log::Field(err);

            continue;
        }
    }

    return ErrorEnum::eNone;
}

Error StorageState::PrepareState(const InstanceIdent& instanceIdent, const SetupParams& setupParams,
    const Array<uint8_t>& checksum, String& statePath)
{
    LOG_DBG() << "Prepare state";

    if (setupParams.mStateQuota == 0) {
        if (auto err = fs::RemoveAll(GetStateDir(instanceIdent)); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    if (auto err = fs::MakeDirAll(GetStateDir(instanceIdent)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = CreateStateFileIfNotExist(GetStateFilePath(instanceIdent), setupParams); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = StartStateWatching(instanceIdent, GetStateFilePath(instanceIdent), setupParams.mStateQuota);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Error err;
    auto  stopOnError = DeferRelease(&err, [this, instanceIdent](auto* err) {
        if (!err->IsNone()) {
            if (auto stopErr = StopStateWatching(instanceIdent);
                !stopErr.IsNone() && !stopErr.Is(ErrorEnum::eNotFound)) {
                LOG_ERR() << "Failed stopping state watching" << Log::Field("instanceIdent", instanceIdent)
                          << Log::Field(stopErr);
            }
        }
    });

    auto itState = mStates.FindIf([&instanceIdent](const auto& item) { return item.mInstanceIdent == instanceIdent; });
    if (itState == mStates.end()) {
        err = AOS_ERROR_WRAP(ErrorEnum::eNotFound);

        return err;
    }

    err = itState->mChecksum.Assign(checksum);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = CheckChecksumAndSendUpdateRequest(*itState);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = ToRelativePath(mConfig.mStateDir, GetStateFilePath(instanceIdent), statePath);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StorageState::PrepareStorage(
    const InstanceIdent& instanceIdent, const SetupParams& setupParams, String& storagePath) const
{
    const auto fullPath = GetStoragePath(instanceIdent);

    LOG_DBG() << "Prepare storage" << Log::Field("path", fullPath);

    if (setupParams.mStorageQuota == 0) {
        if (auto err = fs::RemoveAll(fullPath); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    if (auto err = fs::MakeDirAll(fullPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mFSPlatform->ChangeOwner(fullPath, setupParams.mUID, setupParams.mGID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = ToRelativePath(mConfig.mStorageDir, fullPath, storagePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StorageState::CheckChecksumAndSendUpdateRequest(const State& state)
{
    LOG_DBG() << "Check checksum and send update request" << state;

    auto stateContent = MakeUnique<StaticString<cStateLen>>(&mAllocator);

    if (auto err = fs::ReadFileToString(state.mFilePath, *stateContent); !err.IsNone()) {
        return err;
    }

    StaticArray<uint8_t, crypto::cSHA256Size> calculatedChecksum;

    if (auto err = CalculateChecksum(*stateContent, calculatedChecksum); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (state.mChecksum == calculatedChecksum) {
        return ErrorEnum::eNone;
    }

    StateRequest request;

    static_cast<InstanceIdent&>(request) = state.mInstanceIdent;
    request.mDefault                     = false;

    if (auto err = mMessageSender->SendStateRequest(request); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StorageState::CreateStateFileIfNotExist(const String& path, const SetupParams& params) const
{
    if (struct stat buffer {}; stat(path.CStr(), &buffer) == 0) {
        return ErrorEnum::eNone;
    }

    if (auto err = fs::WriteStringToFile(path, "", S_IRUSR | S_IWUSR); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mFSPlatform->ChangeOwner(path, params.mUID, params.mGID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StorageState::StartStateWatching(const InstanceIdent& instanceIdent, const String& path, size_t quota)
{
    LOG_DBG() << "Start state watching" << Log::Field("path", path);

    if (auto err = mFSWatcher->Subscribe(path, *this); !err.IsNone()) {
        return err;
    }

    return mStates.EmplaceBack(instanceIdent, path, quota);
}

Error StorageState::StopStateWatching(const InstanceIdent& instanceIdent)
{
    LOG_DBG() << "Stop state watching" << instanceIdent;

    auto it = mStates.FindIf([&instanceIdent](const auto& item) { return item.mInstanceIdent == instanceIdent; });
    if (it == mStates.end()) {
        return ErrorEnum::eNotFound;
    }

    auto err = mFSWatcher->Unsubscribe(it->mFilePath.CStr(), *this);

    mStates.Erase(it);

    return err;
}

Error StorageState::SetQuotas(const SetupParams& setupParams)
{
    LOG_DBG() << "Set quotas" << setupParams;

    if (mStateAndStorageOnSamePartition) {
        if (auto err = mFSPlatform->SetUserQuota(
                mConfig.mStorageDir, setupParams.mStorageQuota + setupParams.mStateQuota, setupParams.mUID);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    if (auto err = mFSPlatform->SetUserQuota(mConfig.mStateDir, setupParams.mStateQuota, setupParams.mUID);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mFSPlatform->SetUserQuota(mConfig.mStorageDir, setupParams.mStorageQuota, setupParams.mUID);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StorageState::SendNewStateIfFileChanged(State& state)
{
    auto newState = MakeUnique<NewState>(&mAllocator);

    static_cast<InstanceIdent&>(*newState) = state.mInstanceIdent;

    if (auto err = fs::ReadFileToString(state.mFilePath, newState->mState); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = CalculateChecksum(newState->mState, newState->mChecksum); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (state.mChecksum == newState->mChecksum) {
        return ErrorEnum::eNone;
    }

    if (auto err = state.mChecksum.Assign(newState->mChecksum); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mMessageSender->SendNewState(*newState); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StorageState::RemoveFromSystem(const InstanceIdent& instanceIdent)
{
    const auto stateDir    = GetStateDir(instanceIdent);
    const auto storagePath = GetStoragePath(instanceIdent);

    LOG_DBG() << "Remove storage and state from system" << Log::Field("instanceIdent", instanceIdent)
              << Log::Field("statePath", stateDir) << Log::Field("storagePath", storagePath);

    if (auto err = fs::RemoveAll(stateDir); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::RemoveAll(storagePath); !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorage->RemoveStorageStateInfo(instanceIdent); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool StorageState::QuotasAreEqual(const InstanceInfo& lhs, const SetupParams& rhs) const
{
    return lhs.mStorageQuota == rhs.mStorageQuota && lhs.mStateQuota == rhs.mStateQuota;
}

Error StorageState::ValidateChecksum(const String& text, const Array<uint8_t>& checksum)
{
    StaticArray<uint8_t, crypto::cSHA256Size> calculatedChecksum;

    if (auto err = CalculateChecksum(text, calculatedChecksum); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (calculatedChecksum != checksum) {
        return ErrorEnum::eInvalidChecksum;
    }

    return ErrorEnum::eNone;
}

StaticString<cFilePathLen> StorageState::GetStateDir(const InstanceIdent& instanceIdent) const
{
    StaticString<cInstanceStringLen> instanceStr;

    instanceStr.Convert(instanceIdent.mInstance);

    auto path = fs::JoinPath(mConfig.mStateDir, instanceIdent.mItemID, instanceIdent.mSubjectID, instanceStr);

    return path;
}

StaticString<cFilePathLen> StorageState::GetStateFilePath(const InstanceIdent& instanceIdent) const
{
    auto path = GetStateDir(instanceIdent);

    return fs::AppendPath(path, cStateFilename);
}

StaticString<cFilePathLen> StorageState::GetStoragePath(const InstanceIdent& instanceIdent) const
{
    StaticString<cInstanceStringLen> instanceStr;

    instanceStr.Convert(instanceIdent.mInstance);

    return fs::JoinPath(mConfig.mStorageDir, instanceIdent.mItemID, instanceIdent.mSubjectID, instanceStr);
}

Error StorageState::CalculateChecksum(const String& data, Array<uint8_t>& checksum)
{
    auto [hasher, err] = mHasher->CreateHash(cHashAlgorithm);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = hasher->Update({reinterpret_cast<const uint8_t*>(data.CStr()), data.Size()});
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = hasher->Finalize(checksum);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::storagestate
