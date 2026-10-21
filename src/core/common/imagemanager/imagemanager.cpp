/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "imagemanager.hpp"

namespace aos::common::imagemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ImageManager::Start()
{
    LOG_DBG() << "Start image manager";

    return ErrorEnum::eNotSupported;
}

Error ImageManager::Stop()
{
    LOG_DBG() << "Stop image manager";

    return ErrorEnum::eNotSupported;
}

Error ImageManager::GetItemsStatuses(Array<UpdateItemStatus>& statuses)
{
    LOG_DBG() << "Get items statuses";

    auto items = MakeUnique<StaticArray<DeployableItemInfo, cMaxNumStoredItems>>(mAllocator);
    if (!items) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    LockGuard lock {mMutex};

    if (auto err = mStorage->GetAllDeployableItems(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& item : *items) {
        UpdateItemStatus status;

        status.mItemID       = item.mItemID;
        status.mType         = item.mType;
        status.mVersion      = item.mVersion;
        status.mPreinstalled = false;

        if (item.mState == ItemStateEnum::eInstalling) {
            status.mState = ItemStateEnum::eFailed;
        } else {
            status.mState = item.mState;

            if (item.mState == ItemStateEnum::eFailed) {
                status.mError = item.mError;
            }
        }

        if (auto err = statuses.PushBack(status); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::SubscribeListener(ItemStatusListenerItf& listener)
{
    LockGuard lock {mListenersMutex};

    LOG_DBG() << "Subscribe listener";

    if (mListeners.Contains(&listener)) {
        return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
    }

    if (auto err = mListeners.PushBack(&listener); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::UnsubscribeListener(ItemStatusListenerItf& listener)
{
    LockGuard lock {mListenersMutex};

    LOG_DBG() << "Unsubscribe listener";

    if (mListeners.Remove(&listener) == 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetBlobPath(const String& digest, String& path) const
{
    LOG_DBG() << "Get blob path" << Log::Field("digest", digest);

    StaticString<oci::cDigestLen> algorithm;
    StaticString<oci::cDigestLen> hash;

    if (auto err = ParseDigest(digest, algorithm, hash); !err.IsNone()) {
        return err;
    }

    if (auto err = path.Assign(fs::JoinPath(mConfig.mImagePath, cBlobsFolder, algorithm, hash)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Protected
 **********************************************************************************************************************/

Error ImageManager::Init(const Config& config, StorageItf& storage, spaceallocator::SpaceAllocatorItf& spaceAllocator,
    fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec, fs::FSPlatformItf& fsPlatform,
    AllocatorItf& allocator)
{
    LOG_DBG() << "Init image manager";

    mConfig           = config;
    mStorage          = &storage;
    mSpaceAllocator   = &spaceAllocator;
    mFileInfoProvider = &fileInfoProvider;
    mOCISpec          = &ociSpec;
    mFSPlatform       = &fsPlatform;
    mAllocator        = &allocator;

    LOG_DBG() << "Config" << Log::Field("imagePath", mConfig.mImagePath) << Log::Field("partLimit", mConfig.mPartLimit)
              << Log::Field("itemCacheTTL", mConfig.mItemCacheTTL)
              << Log::Field("removeOutdatedPeriod", mConfig.mRemoveOutdatedPeriod);

    if (auto err = fs::MakeDirAll(fs::JoinPath(mConfig.mImagePath, cBlobsFolder)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallItem(const InstallItemInfo& itemInfo)
{
    LOG_DBG() << "Install item" << Log::Field("itemID", itemInfo.mItemID) << Log::Field("version", itemInfo.mVersion)
              << Log::Field("rootDigest", itemInfo.mRootDigest);

    return ErrorEnum::eNotSupported;
}

Error ImageManager::RemoveItem(const String& itemID, const String& version)
{
    LOG_DBG() << "Remove item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    return ErrorEnum::eNotSupported;
}

Error ImageManager::ValidateBlob(const String& digest, const String& path) const
{
    LOG_DBG() << "Validate blob" << Log::Field("digest", digest) << Log::Field("path", path);

    StaticString<oci::cDigestLen> algorithm;
    StaticString<oci::cDigestLen> hash;

    if (auto err = ParseDigest(digest, algorithm, hash); !err.IsNone()) {
        return err;
    }

    if (algorithm != cSHA256DigestAlgorithm) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotSupported, "unsupported digest algorithm"));
    }

    StaticArray<uint8_t, crypto::cSHA256Size> hashBytes;

    if (hash.Size() != crypto::cSHA256Size * 2 || !hash.HexToByteArray(hashBytes).IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest hash"));
    }

    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!exists) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "blob not found"));
    }

    fs::FileInfo fileInfo;

    if (err = mFileInfoProvider->GetFileInfo(path, fileInfo, crypto::HashEnum::eSHA256); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (fileInfo.mCheckSum != hashBytes) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidChecksum);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::DownloadBlob(const String& digest, const String& path)
{
    return DoDownloadBlob(digest, path);
}

Error ImageManager::InstallLayer(const oci::ContentDescriptor& descriptor, const String& diffDigest, const String& path)
{
    LOG_DBG() << "Install layer" << Log::Field("digest", descriptor.mDigest) << Log::Field("diffDigest", diffDigest)
              << Log::Field("path", path) << Log::Field("size", descriptor.mSize);

    StaticString<cFilePathLen> parentPath;

    if (auto err = fs::ParentPath(path, parentPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::MakeDirAll(parentPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::RemoveAll(path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto [space, err] = GetSpaceAllocator().AllocateSpace(descriptor.mSize);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = DownloadBlob(descriptor.mDigest, path);

    if (err.IsNone()) {
        err = ValidateBlob(descriptor.mDigest, path);
    }

    if (!err.IsNone()) {
        LOG_ERR() << "Failed to install layer" << Log::Field("digest", descriptor.mDigest) << Log::Field(err);

        if (auto removeErr = fs::RemoveAll(path); !removeErr.IsNone()) {
            LOG_ERR() << "Failed to remove layer blob" << Log::Field("path", path) << Log::Field(removeErr);
        }

        if (auto releaseErr = space->Release(); !releaseErr.IsNone()) {
            LOG_ERR() << "Failed to release space" << Log::Field(AOS_ERROR_WRAP(releaseErr));
        }

        return AOS_ERROR_WRAP(err);
    }

    if (auto acceptErr = space->Accept(); !acceptErr.IsNone()) {
        LOG_ERR() << "Can't accept space" << Log::Field(AOS_ERROR_WRAP(acceptErr));
    }

    return ErrorEnum::eNone;
}

RetWithError<size_t> ImageManager::RemoveLayer(
    const String& digest, const String& mediaType, const String& diffID, const String& path)
{
    LOG_DBG() << "Remove layer" << Log::Field("digest", digest) << Log::Field("mediaType", mediaType)
              << Log::Field("diffID", diffID) << Log::Field("path", path);

    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (!exists) {
        return {0, ErrorEnum::eNone};
    }

    size_t size = 0;

    if (Tie(size, err) = fs::CalculateSize(*mAllocator, path); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (err = fs::RemoveAll(path); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    return {size, ErrorEnum::eNone};
}

Error ImageManager::ValidateLayer(
    const oci::ContentDescriptor& descriptor, const String& diffDigest, const String& path)
{
    LOG_DBG() << "Validate layer" << Log::Field("digest", descriptor.mDigest) << Log::Field("diffDigest", diffDigest)
              << Log::Field("path", path);

    return ValidateBlob(descriptor.mDigest, path);
}

void ImageManager::NotifyItemStatusChanged(const UpdateItemStatus& status)
{
    LOG_DBG() << "Item status changed" << Log::Field("itemID", status.mItemID) << Log::Field("type", status.mType)
              << Log::Field("version", status.mVersion) << Log::Field("state", status.mState)
              << Log::Field(status.mError);

    StaticArray<ItemStatusListenerItf*, cMaxNumListeners> listeners;

    {
        LockGuard lock {mListenersMutex};

        listeners = mListeners;
    }

    for (auto* listener : listeners) {
        listener->OnItemStatusChanged(status);
    }
}

void ImageManager::NotifyItemRemovedFromCache(const String& itemID, const String& version)
{
    LOG_DBG() << "Item removed from cache" << Log::Field("itemID", itemID) << Log::Field("version", version);

    StaticArray<ItemStatusListenerItf*, cMaxNumListeners> listeners;

    {
        LockGuard lock {mListenersMutex};

        listeners = mListeners;
    }

    for (auto* listener : listeners) {
        listener->OnItemRemovedFromCache(itemID, version);
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

RetWithError<size_t> ImageManager::RemoveOutdatedItem(const String& itemID, const String& version)
{
    LOG_DBG() << "Remove outdated item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    return {0, ErrorEnum::eNotSupported};
}

Error ImageManager::ParseDigest(const String& digest, String& algorithm, String& hash)
{
    auto [pos, err] = digest.FindSubstr(0, ":");
    if (!err.IsNone() || pos == 0 || pos + 1 >= digest.Size()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
    }

    if (err = algorithm.Assign(String(digest.CStr(), pos)); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
    }

    if (err = hash.Assign(String(digest.CStr() + pos + 1, digest.Size() - pos - 1)); !err.IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
    }

    // Allowed characters are taken from OCI image spec digest grammar: algorithm is [a-z0-9]+ separated by [+._-],
    // encoded is [a-zA-Z0-9=_-]+. Uppercase is rejected in encoded part to keep digests canonical, so the same blob
    // can't map to two different paths. Restricted character set also prevents path traversal as digest parts are
    // used as path components.
    const auto isLowerAlnum = [](char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'); };

    if (!isLowerAlnum(algorithm[0])) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
    }

    for (const auto ch : algorithm) {
        if (!(isLowerAlnum(ch) || ch == '+' || ch == '.' || ch == '_' || ch == '-')) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
        }
    }

    for (const auto ch : hash) {
        if (!(isLowerAlnum(ch) || ch == '=' || ch == '_' || ch == '-')) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "malformed digest"));
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::common::imagemanager
