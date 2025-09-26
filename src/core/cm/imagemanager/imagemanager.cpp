/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>
#include <core/common/tools/semver.hpp>

#include "imagemanager.hpp"

namespace aos::cm::imagemanager {

/**********************************************************************************************************************
 * Public
 ***********************************************************************************************************************/

Error ImageManager::Init(const Config& config, storage::StorageItf& storage,
    spaceallocator::SpaceAllocatorItf& spaceAllocator, fileserver::FileServerItf& fileserver,
    crypto::CryptoHelperItf& imageDecrypter, fs::FileInfoProviderItf& fileInfoProvider)
{
    LOG_DBG() << "Init image manager";

    mConfig           = config;
    mStorage          = &storage;
    mSpaceAllocator   = &spaceAllocator;
    mFileServer       = &fileserver;
    mImageDecrypter   = &imageDecrypter;
    mFileInfoProvider = &fileInfoProvider;

    return SetOutdatedItems();
}

Error ImageManager::Start()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start image manager";

    if (auto err = RemoveOutdatedItems(); !err.IsNone()) {
        return err;
    }

    if (auto err = mTimer.Start(
            cRemovePeriod,
            [this](void*) {
                if (auto err = RemoveOutdatedItems(); !err.IsNone()) {
                    LOG_ERR() << "Error removing outdated items" << Log::Field(err);
                }
            },
            false);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::Stop()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Stop image manager";

    mActionPool.Shutdown();

    return mTimer.Stop();
}

Error ImageManager::GetUpdateItemsStatuses(Array<UpdateItemStatus>& statuses)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get update items statuses";

    auto items = MakeUnique<StaticArray<storage::ItemInfo, cMaxNumUpdateItems>>(&mAllocator);

    if (auto err = mStorage->GetItemsInfo(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& item : *items) {
        if (item.mState != storage::ItemStateEnum::eActive) {
            continue;
        }

        if (auto err = statuses.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& status = statuses.Back();

        status.mID      = item.mID;
        status.mVersion = item.mVersion;

        for (const auto& image : item.mImages) {
            if (auto err = status.mStatuses.EmplaceBack(); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            auto& imageStatus = status.mStatuses.Back();

            imageStatus.mImageID = image.mImageID;
            imageStatus.mState   = ImageStateEnum::eInstalled;
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallUpdateItems(const Array<UpdateItemInfo>& itemsInfo,
    const Array<cloudprotocol::CertificateInfo>&                    certificates,
    const Array<cloudprotocol::CertificateChainInfo>& certificateChains, Array<UpdateItemStatus>& statuses)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Install update items" << Log::Field("count", itemsInfo.Size());

    if (auto err = statuses.Resize(itemsInfo.Size()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mActionPool.Run(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (size_t i = 0; i < itemsInfo.Size(); ++i) {
        auto& itemInfo = itemsInfo[i];
        auto& status   = statuses[i];

        auto err = mActionPool.AddTask([this, &itemInfo, &certificates, &certificateChains, &status](void*) {
            if (auto err = InstallUpdateItem(itemInfo, certificates, certificateChains, status); !err.IsNone()) {
                LOG_ERR() << "Can't install update item" << Log::Field("id", itemInfo.mID)
                          << Log::Field("version", itemInfo.mVersion) << Log::Field(err);
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't add update item install task" << Log::Field("id", itemInfo.mID)
                      << Log::Field("version", itemInfo.mVersion) << Log::Field(err);
        }
    }

    mActionPool.Wait();
    mActionPool.Shutdown();

    return ErrorEnum::eNone;
}

Error ImageManager::UninstallUpdateItems(const Array<StaticString<cIDLen>>& ids, Array<UpdateItemStatus>& statuses)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Uninstall update items" << Log::Field("count", ids.Size());

    if (auto err = statuses.Resize(ids.Size()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mActionPool.Run(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (size_t i = 0; i < ids.Size(); ++i) {
        auto& id     = ids[i];
        auto& status = statuses[i];

        auto err = mActionPool.AddTask([this, &id, &status](void*) {
            if (auto err = UninstallUpdateItem(id, status); !err.IsNone()) {
                LOG_ERR() << "Can't uninstall update item" << Log::Field("id", id) << Log::Field(err);
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't add update item uninstall task" << Log::Field("id", id) << Log::Field(err);
        }
    }

    mActionPool.Wait();
    mActionPool.Shutdown();

    return ErrorEnum::eNone;
}

Error ImageManager::RevertUpdateItems(const Array<StaticString<cIDLen>>& ids, Array<UpdateItemStatus>& statuses)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Revert update items" << Log::Field("count", ids.Size());

    if (auto err = mActionPool.Run(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& id : ids) {
        auto err = mActionPool.AddTask([this, &id, &statuses](void*) {
            if (auto err = RevertUpdateItem(id, statuses); !err.IsNone()) {
                LOG_ERR() << "Can't revert update item" << Log::Field("id", id) << Log::Field(err);
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't add update item revert task" << Log::Field("id", id) << Log::Field(err);
        }
    }

    mActionPool.Wait();
    mActionPool.Shutdown();

    return ErrorEnum::eNone;
}

Error ImageManager::SubscribeListener(StatusListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe listener";

    auto it = mListeners.Find(&listener);

    if (it != mListeners.end()) {
        return ErrorEnum::eAlreadyExist;
    }

    return AOS_ERROR_WRAP(mListeners.PushBack(&listener));
}

Error ImageManager::UnsubscribeListener(StatusListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unsubscribe listener";

    return mListeners.Remove(&listener) > 0 ? ErrorEnum::eNone : ErrorEnum::eNotFound;
}

Error ImageManager::GetUpdateImageInfo(
    const String& id, const PlatformInfo& platform, smcontroller::UpdateImageInfo& info)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get update image info" << Log::Field("id", id)
              << Log::Field("architecture", platform.mArchInfo.mArchitecture) << Log::Field("os", platform.mOSInfo.mOS);

    auto items = MakeUnique<StaticArray<storage::ItemInfo, cMaxItemVersions>>(&mAllocator);

    if (auto err = mStorage->GetItemVersionsByID(id, *items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto it = items->FindIf(
        [&id](const storage::ItemInfo& item) { return item.mState == storage::ItemStateEnum::eActive; });
    if (it == items->end()) {
        return ErrorEnum::eNotFound;
    }

    for (const auto& image : it->mImages) {
        if (image.mArchInfo == platform.mArchInfo && image.mOSInfo == platform.mOSInfo) {
            info.mImageID = image.mImageID;
            info.mVersion = it->mVersion;
            info.mURL     = image.mURL;
            info.mSHA256  = image.mSHA256;
            info.mSize    = image.mSize;

            return ErrorEnum::eNone;
        }
    }

    return ErrorEnum::eNotFound;
}

Error ImageManager::GetLayerImageInfo(
    [[maybe_unused]] const String& digest, [[maybe_unused]] smcontroller::UpdateImageInfo& info)
{
    return ErrorEnum::eNone;
}

RetWithError<StaticString<cVersionLen>> ImageManager::GetItemVersion(const String& id)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get item version" << Log::Field("id", id);

    auto items = MakeUnique<StaticArray<storage::ItemInfo, cMaxItemVersions>>(&mAllocator);

    if (auto err = mStorage->GetItemVersionsByID(id, *items); !err.IsNone()) {
        return {StaticString<cVersionLen> {}, AOS_ERROR_WRAP(err)};
    }

    auto it = items->FindIf(
        [&id](const storage::ItemInfo& item) { return item.mState == storage::ItemStateEnum::eActive; });
    if (it == items->end()) {
        return {StaticString<cVersionLen> {}, ErrorEnum::eNotFound};
    }

    return {it->mVersion, ErrorEnum::eNone};
}

Error ImageManager::GetItemImages(const String& id, Array<ImageInfo>& imagesInfos)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get item images" << Log::Field("id", id);

    auto items = MakeUnique<StaticArray<storage::ItemInfo, cMaxItemVersions>>(&mAllocator);

    if (auto err = mStorage->GetItemVersionsByID(id, *items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& item : *items) {
        if (item.mState != storage::ItemStateEnum::eActive) {
            continue;
        }

        for (const auto& image : item.mImages) {
            if (auto err = imagesInfos.EmplaceBack(); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            auto& imageInfo = imagesInfos.Back();

            imageInfo.mImageID  = image.mImageID;
            imageInfo.mArchInfo = image.mArchInfo;
            imageInfo.mOSInfo   = image.mOSInfo;
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetServiceConfig([[maybe_unused]] const String& id, [[maybe_unused]] const String& imageID,
    [[maybe_unused]] oci::ServiceConfig& config)
{
    return ErrorEnum::eNone;
}

Error ImageManager::GetImageConfig([[maybe_unused]] const String& id, [[maybe_unused]] const String& imageID,
    [[maybe_unused]] oci::ImageConfig& config)
{
    return ErrorEnum::eNone;
}

Error ImageManager::RemoveItem(const String& id)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Remove item" << Log::Field("id", id);

    auto items = MakeUnique<StaticArray<storage::ItemInfo, cMaxItemVersions>>(&mAllocator);

    if (auto err = mStorage->GetItemVersionsByID(id, *items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& item : *items) {
        if (auto removeErr = Remove(item); !removeErr.IsNone()) {
            return removeErr;
        }
    }

    return ErrorEnum::eNone;
}

/**********************************************************************************************************************
 * Private
 ***********************************************************************************************************************/

Error ImageManager::InstallUpdateItem(const UpdateItemInfo& itemInfo,
    const Array<cloudprotocol::CertificateInfo>&            certificates,
    const Array<cloudprotocol::CertificateChainInfo>& certificateChains, UpdateItemStatus& status)
{
    LOG_DBG() << "Install update item" << Log::Field("id", itemInfo.mID) << Log::Field("version", itemInfo.mVersion)
              << Log::Field("images", itemInfo.mImages.Size());

    Error err;
    auto  items = MakeUnique<StaticArray<storage::ItemInfo, cMaxItemVersions>>(&mAllocator);

    auto fillStatus = DeferRelease(&err, [&](Error* err) {
        status.mID      = itemInfo.mID;
        status.mVersion = itemInfo.mVersion;

        if (err->IsNone() || !status.mStatuses.IsEmpty()) {
            return;
        }

        for (const auto& image : itemInfo.mImages) {
            if (auto errEmplace = status.mStatuses.EmplaceBack(); !errEmplace.IsNone()) {
                LOG_ERR() << "Can't emplace back update item status" << Log::Field("id", itemInfo.mID)
                          << Log::Field("version", itemInfo.mVersion) << Log::Field(errEmplace);

                return;
            }

            auto& imageStatus = status.mStatuses.Back();

            imageStatus.mImageID = image.mImage.mImageID;
            imageStatus.mState   = ImageStateEnum::eFailed;
            imageStatus.mError   = *err;

            NotifyImageStatusChangedListeners(imageStatus);
        }
    });

    if (err = mStorage->GetItemVersionsByID(itemInfo.mID, *items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = ValidateActiveVersionItem(itemInfo, *items); !err.IsNone()) {
        if (!err.Is(ErrorEnum::eNotFound)) {
            return err;
        }

        if (err = ValidateCachedVersionItem(itemInfo, *items); !err.IsNone()) {
            if (!err.Is(ErrorEnum::eNotFound)) {
                return err;
            }
        }
    }

    size_t totalSize {};

    for (const auto& image : itemInfo.mImages) {
        totalSize += image.mSize;
    }

    UniquePtr<spaceallocator::SpaceItf> space;

    Tie(space, err) = mSpaceAllocator->AllocateSpace(totalSize);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto itemPath    = fs::JoinPath("items", itemInfo.mVersion);
    auto installPath = fs::JoinPath(mConfig.mInstallPath, itemPath);

    // cppcheck-suppress constParameterPointer
    auto releaseItemSpace = DeferRelease(&err, [&](Error* err) {
        if (!err->IsNone()) {
            LOG_ERR() << "Can't install item" << Log::Field("id", itemInfo.mID)
                      << Log::Field("version", itemInfo.mVersion) << Log::Field(*err);

            ReleaseAllocatedSpace(installPath, space.Get());

            return;
        }

        AcceptAllocatedSpace(space.Get());
    });

    auto item = MakeUnique<storage::ItemInfo>(&mAllocator);

    item->mID        = itemInfo.mID;
    item->mVersion   = itemInfo.mVersion;
    item->mState     = storage::ItemStateEnum::eActive;
    item->mPath      = installPath;
    item->mTotalSize = totalSize;
    item->mTimestamp = Time::Now();

    for (const auto& image : itemInfo.mImages) {
        if (err = status.mStatuses.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& imageStatus    = status.mStatuses.Back();
        imageStatus.mImageID = image.mImage.mImageID;

        auto setImageStatus = DeferRelease(&err, [&](Error* err) {
            if (!err->IsNone()) {
                imageStatus.mState = ImageStateEnum::eFailed;
            } else {
                imageStatus.mState = ImageStateEnum::eInstalled;
            }

            imageStatus.mError = *err;

            NotifyImageStatusChangedListeners(imageStatus);
        });

        if (err = item->mImages.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (err = InstallImage(image, installPath, itemPath, item->mImages.Back(), certificateChains, certificates);
            !err.IsNone()) {
            return err;
        }
    }

    if (err = UpdatePrevVersions(*items); !err.IsNone()) {
        return err;
    }

    if (err = mStorage->AddItem(*item); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::UninstallUpdateItem(const String& id, UpdateItemStatus& status)
{
    LOG_DBG() << "Uninstall update item" << Log::Field("id", id);

    auto items = MakeUnique<StaticArray<storage::ItemInfo, cMaxItemVersions>>(&mAllocator);

    if (auto err = mStorage->GetItemVersionsByID(id, *items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Error err;

    for (const auto& item : *items) {
        status.mID      = item.mID;
        status.mVersion = item.mVersion;

        auto setItemStatus = DeferRelease(&err, [&](const Error* err) {
            if (auto errStatus = SetItemStatus(item.mImages, status, ImageStateEnum::eRemoved, *err);
                !errStatus.IsNone()) {
                LOG_ERR() << "Can't set update item status" << Log::Field("id", item.mID)
                          << Log::Field("version", item.mVersion) << Log::Field(errStatus);

                return;
            }
        });

        switch (item.mState.GetValue()) {
        case storage::ItemStateEnum::eActive:
            if (auto setStateErr = SetState(item, storage::ItemStateEnum::eCached); !setStateErr.IsNone()) {
                return setStateErr;
            }

            break;

        case storage::ItemStateEnum::eCached:
            if (auto removeErr = Remove(item); !removeErr.IsNone()) {
                return removeErr;
            }

            break;

        default:
            return ErrorEnum::eInvalidArgument;
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::RevertUpdateItem(const String& id, Array<UpdateItemStatus>& statuses)
{
    LOG_DBG() << "Revert update item" << Log::Field("id", id);

    auto items = MakeUnique<StaticArray<storage::ItemInfo, cMaxItemVersions>>(&mAllocator);

    if (auto err = mStorage->GetItemVersionsByID(id, *items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto itActive
        = items->FindIf([](const storage::ItemInfo& item) { return item.mState == storage::ItemStateEnum::eActive; });

    auto itCached
        = items->FindIf([](const storage::ItemInfo& item) { return item.mState == storage::ItemStateEnum::eCached; });

    auto createStatus = [&](const String& id, const String& version) -> RetWithError<UpdateItemStatus*> {
        if (auto err = statuses.EmplaceBack(); !err.IsNone()) {
            return {nullptr, AOS_ERROR_WRAP(err)};
        }

        auto& status = statuses.Back();

        status.mID      = id;
        status.mVersion = version;

        return {&status, ErrorEnum::eNone};
    };

    if (itActive != items->end()) {
        {
            auto [status, err] = createStatus(itActive->mID, itActive->mVersion);
            if (!err.IsNone()) {
                return err;
            }

            err = Remove(*itActive);

            if (auto errStatus = SetItemStatus(itActive->mImages, *status, ImageStateEnum::eRemoved, err);
                !errStatus.IsNone()) {
                return errStatus;
            }

            if (!err.IsNone()) {
                return err;
            }
        }

        if (itCached != items->end()) {
            {
                auto [status, err] = createStatus(itCached->mID, itCached->mVersion);
                if (!err.IsNone()) {
                    return err;
                }

                err = SetState(*itCached, storage::ItemStateEnum::eActive);

                if (auto errStatus = SetItemStatus(itCached->mImages, *status, ImageStateEnum::eInstalled, err);
                    !errStatus.IsNone()) {
                    return errStatus;
                }

                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::SetItemStatus(
    const Array<storage::ImageInfo>& itemImages, UpdateItemStatus& status, ImageState state, Error error)
{
    for (const auto& itemImage : itemImages) {
        if (auto err = status.mStatuses.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& imageStatus = status.mStatuses.Back();

        imageStatus.mImageID = itemImage.mImageID;
        imageStatus.mState   = error.IsNone() ? state.GetValue() : ImageStateEnum::eFailed;
        imageStatus.mError   = error;

        NotifyImageStatusChangedListeners(imageStatus);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ValidateActiveVersionItem(const UpdateItemInfo& itemInfo, const Array<storage::ItemInfo>& items)
{
    LOG_DBG() << "Validate active version item" << Log::Field("id", itemInfo.mID)
              << Log::Field("version", itemInfo.mVersion);

    auto it = items.FindIf(
        [&itemInfo](const storage::ItemInfo& item) { return item.mState == storage::ItemStateEnum::eActive; });

    if (it == items.end()) {
        return ErrorEnum::eNotFound;
    }

    auto [versionResult, err] = semver::CompareSemver(itemInfo.mVersion, it->mVersion);

    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (versionResult == 0) {
        return ErrorEnum::eAlreadyExist;
    }

    if (versionResult < 0) {
        return ErrorEnum::eWrongState;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ValidateCachedVersionItem(const UpdateItemInfo& itemInfo, const Array<storage::ItemInfo>& items)
{
    LOG_DBG() << "Validate cached version item" << Log::Field("id", itemInfo.mID)
              << Log::Field("version", itemInfo.mVersion);

    auto it = items.FindIf(
        [&itemInfo](const storage::ItemInfo& item) { return item.mState == storage::ItemStateEnum::eCached; });

    if (it == items.end()) {
        return ErrorEnum::eNotFound;
    }

    auto [versionResult, err] = semver::CompareSemver(itemInfo.mVersion, it->mVersion);

    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (versionResult == 0) {
        if (auto setStateErr = SetState(*it, storage::ItemStateEnum::eActive); !setStateErr.IsNone()) {
            return setStateErr;
        }

        return ErrorEnum::eAlreadyExist;
    }

    if (versionResult > 0) {
        if (auto removeErr = Remove(*it); !removeErr.IsNone()) {
            return removeErr;
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::SetState(const storage::ItemInfo& item, storage::ItemState state)
{
    LOG_DBG() << "Set state" << Log::Field("id", item.mID) << Log::Field("version", item.mVersion)
              << Log::Field("state", state);

    if (auto err = mStorage->SetItemState(item.mID, item.mVersion, state); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (state == storage::ItemStateEnum::eCached) {
        if (auto err = mSpaceAllocator->AddOutdatedItem(item.mID, item.mTotalSize, item.mTimestamp); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (item.mState == storage::ItemStateEnum::eCached) {
        if (auto err = mSpaceAllocator->RestoreOutdatedItem(item.mID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::Remove(const storage::ItemInfo& item)
{
    LOG_DBG() << "Removing item" << Log::Field("id", item.mID) << Log::Field("version", item.mVersion);

    NotifyItemRemovedListeners(item.mID);

    if (auto err = fs::RemoveAll(item.mPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (item.mState == storage::ItemStateEnum::eCached) {
        if (auto err = mSpaceAllocator->RestoreOutdatedItem(item.mID); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    mSpaceAllocator->FreeSpace(item.mTotalSize);

    if (auto err = mStorage->RemoveItem(item.mID, item.mVersion); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Item removed" << Log::Field("id", item.mID) << Log::Field("version", item.mVersion);

    return ErrorEnum::eNone;
}

void ImageManager::NotifyItemRemovedListeners(const String& id)
{
    LOG_DBG() << "Notify item removed listeners" << Log::Field("id", id);

    for (const auto& listener : mListeners) {
        listener->OnUpdateItemRemoved(id);
    }
}

void ImageManager::NotifyImageStatusChangedListeners(const ImageStatus& status)
{
    LOG_DBG() << "Notify image status changed listeners" << Log::Field("imageID", status.mImageID)
              << Log::Field("state", status.mState);

    for (const auto& listener : mListeners) {
        listener->OnImageStatusChanged(status.mImageID, status);
    }
}

void ImageManager::ReleaseAllocatedSpace(const String& path, spaceallocator::SpaceItf* space)
{
    if (!path.IsEmpty()) {
        fs::RemoveAll(path);
    }

    if (auto err = space->Release(); !err.IsNone()) {
        LOG_ERR() << "Can't release item space: err=" << err;
    }
}

void ImageManager::AcceptAllocatedSpace(spaceallocator::SpaceItf* space)
{
    if (auto err = space->Accept(); !err.IsNone()) {
        LOG_ERR() << "Can't accept item space: err=" << err;
    }
}

Error ImageManager::InstallImage(const UpdateImageInfo& imageInfo, const String& installPath, const String& layerPath,
    storage::ImageInfo& image, const Array<cloudprotocol::CertificateChainInfo>& certificateChains,
    const Array<cloudprotocol::CertificateInfo>& certificates)
{
    LOG_DBG() << "Install image item" << Log::Field("id", imageInfo.mImage.mImageID);

    StaticString<cFilePathLen> decryptedFile;

    if (auto err = DecryptAndValidate(imageInfo, installPath, certificateChains, certificates, decryptedFile);
        !err.IsNone()) {
        return err;
    }

    image.mImageID = imageInfo.mImage.mImageID;
    image.mPath    = decryptedFile;

    if (auto err = PrepareURLsAndFileInfo(layerPath, decryptedFile, imageInfo, image); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::DecryptAndValidate(const UpdateImageInfo& imageInfo, const String& installPath,
    const Array<cloudprotocol::CertificateChainInfo>& certificateChains,
    const Array<cloudprotocol::CertificateInfo>& certificates, StaticString<cFilePathLen>& outDecryptedFile)
{
    outDecryptedFile = fs::JoinPath(installPath, imageInfo.mImage.mImageID);

    if (auto err = mImageDecrypter->Decrypt(imageInfo.mPath, outDecryptedFile, imageInfo.mDecryptInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err
        = mImageDecrypter->ValidateSigns(outDecryptedFile, imageInfo.mSignInfo, certificateChains, certificates);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::PrepareURLsAndFileInfo(
    const String& imagePath, const String& decryptedFile, const UpdateImageInfo& imageInfo, storage::ImageInfo& image)
{
    StaticString<cFilePathLen> remoteURL;
    StaticString<cFilePathLen> localURL;
    StaticString<cFilePathLen> imageBaseName;

    if (auto err = fs::BaseName(decryptedFile, imageBaseName); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    fs::FileInfo fileInfo;

    if (auto err = mFileInfoProvider->CreateFileInfo(decryptedFile, fileInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (fileInfo.mSHA256 != imageInfo.mSHA256) {
        return ErrorEnum::eInvalidChecksum;
    }

    if (fileInfo.mSize != imageInfo.mSize) {
        return ErrorEnum::eInvalidArgument;
    }

    if (auto err = mFileServer->TranslateFilePathURL(fs::JoinPath(imagePath, imageBaseName), remoteURL);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    image.mURL    = remoteURL;
    image.mSize   = fileInfo.mSize;
    image.mSHA256 = fileInfo.mSHA256;

    image.mArchInfo = imageInfo.mImage.mArchInfo;
    image.mOSInfo   = imageInfo.mImage.mOSInfo;

    return ErrorEnum::eNone;
}

Error ImageManager::UpdatePrevVersions(const Array<storage::ItemInfo>& items)
{
    auto itActive
        = items.FindIf([&](const storage::ItemInfo& item) { return item.mState == storage::ItemStateEnum::eActive; });

    if (itActive != items.end()) {
        if (auto err = SetState(*itActive, storage::ItemStateEnum::eCached); !err.IsNone()) {
            return err;
        }
    }

    auto itCached
        = items.FindIf([&](const storage::ItemInfo& item) { return item.mState == storage::ItemStateEnum::eCached; });

    if (itCached != items.end()) {
        if (auto err = Remove(*itCached); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::SetOutdatedItems()
{
    auto items = MakeUnique<StaticArray<storage::ItemInfo, cMaxNumUpdateItems>>(&mAllocator);

    if (auto err = mStorage->GetItemsInfo(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& item : *items) {
        if (item.mState == storage::ItemStateEnum::eCached) {
            if (auto err = mSpaceAllocator->AddOutdatedItem(item.mID, item.mTotalSize, item.mTimestamp);
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::RemoveOutdatedItems()
{
    auto items = MakeUnique<StaticArray<storage::ItemInfo, cMaxNumUpdateItems>>(&mAllocator);

    if (auto err = mStorage->GetItemsInfo(*items); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& item : *items) {
        if (item.mState == storage::ItemStateEnum::eCached
            && item.mTimestamp.Add(mConfig.mUpdateItemTTL) < Time::Now()) {
            if (auto err = Remove(item); !err.IsNone()) {
                return err;
            }
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::imagemanager
