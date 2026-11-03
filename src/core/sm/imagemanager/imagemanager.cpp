/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "imagemanager.hpp"

namespace aos::sm::imagemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ImageManager::Init(const common::imagemanager::Config& config,
    common::imagemanager::BlobInfoProviderItf& blobInfoProvider, StorageItf& storage,
    spaceallocator::SpaceAllocatorItf& spaceAllocator, downloader::DownloaderItf& downloader,
    ImageHandlerItf& imageHandler, fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec,
    fs::FSPlatformItf& fsPlatform, AllocatorItf& allocator)
{
    LOG_DBG() << "Init SM image manager";

    mDownloader   = &downloader;
    mImageHandler = &imageHandler;
    mStorage      = &storage;
    mAllocator    = &allocator;

    // The longest layer path is <image path>/layers/<algorithm>/<hash>/digest, <algorithm>/<hash> has the digest
    // length.
    if (config.mImagePath.Size() + 1 + String(cLayersFolder).Size() + 1 + oci::cDigestLen + 1
            + String(cDigestFile).Size()
        > cFilePathLen) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "image path is too long"));
    }

    if (auto err = common::imagemanager::ImageManager::Init(
            config, blobInfoProvider, storage, spaceAllocator, fileInfoProvider, ociSpec, fsPlatform, allocator);
        !err.IsNone()) {
        return err;
    }

    if (auto err = mLayersPath.Assign(fs::JoinPath(config.mImagePath, cLayersFolder)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::MakeDirAll(mLayersPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallItem(const UpdateItemInfo& itemInfo)
{
    LOG_INF() << "Install item" << Log::Field("itemID", itemInfo.mID) << Log::Field("version", itemInfo.mVersion)
              << Log::Field("type", itemInfo.mType) << Log::Field("manifestDigest", itemInfo.mManifestDigest);

    return common::imagemanager::ImageManager::InstallItem(
        {itemInfo.mID, itemInfo.mType, itemInfo.mVersion, itemInfo.mManifestDigest, false});
}

Error ImageManager::RemoveItem(const String& itemID, const String& version)
{
    LOG_INF() << "Remove item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    return common::imagemanager::ImageManager::RemoveItem(itemID, version);
}

Error ImageManager::Cancel()
{
    LOG_INF() << "Cancel items";

    // The items waiting for a layer processed by another item are canceled as well, so Cancel doesn't wait for the
    // layer unpack.
    SetLayersCanceled(true);

    auto err = common::imagemanager::ImageManager::Cancel();

    SetLayersCanceled(false);

    return err;
}

Error ImageManager::GetLayerPath(const String& diffID, String& path) const
{
    StaticString<cFilePathLen> layerPath;

    if (auto err = CreateLayerPath(diffID, layerPath); !err.IsNone()) {
        return err;
    }

    if (auto err = path.Assign(fs::JoinPath(layerPath, cUnpackedLayerFolder)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Get layer path" << Log::Field("diffID", diffID) << Log::Field("path", path);

    auto [exists, err] = fs::DirExist(path);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!exists) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "unpacked layer not found"));
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error ImageManager::DownloadBlob(CancelContext& ctx, const String& path, const BlobInfo& blobInfo)
{
    LOG_DBG() << "Download blob" << Log::Field("digest", blobInfo.mDigest) << Log::Field("size", blobInfo.mSize);

    // The downloaded blob is validated by the common image manager.
    if (auto err = mDownloader->Download(ctx, blobInfo.mDigest, blobInfo.mURLs, path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ValidateLayer(const String& path, const oci::ContentDescriptor& descriptor, const String& diffID)
{
    (void)path;

    // A layer without diff ID has no image config, so it is kept as packed blob only.
    if (diffID.IsEmpty()) {
        return ErrorEnum::eNotSupported;
    }

    if (auto err = CheckLayerMediaType(descriptor.mMediaType); !err.IsNone()) {
        return err;
    }

    StaticString<cFilePathLen> layerPath;

    if (auto err = CreateLayerPath(diffID, layerPath); !err.IsNone()) {
        return err;
    }

    if (auto err = LockLayer(diffID); !err.IsNone()) {
        return err;
    }

    auto unlockLayer = DeferRelease([this, &diffID]() { UnlockLayer(diffID); });

    if (auto err = ValidateUnpackedLayer(layerPath); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::UnpackLayer(const String& path, const oci::ContentDescriptor& descriptor, const String& diffID)
{
    // ValidateLayer skips the layers without diff ID, so UnpackLayer is never called for them.
    if (diffID.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "layer diff ID is empty"));
    }

    if (auto err = CheckLayerMediaType(descriptor.mMediaType); !err.IsNone()) {
        return err;
    }

    StaticString<cFilePathLen> layerPath;

    if (auto err = CreateLayerPath(diffID, layerPath); !err.IsNone()) {
        return err;
    }

    if (auto err = LockLayer(diffID); !err.IsNone()) {
        return err;
    }

    auto unlockLayer = DeferRelease([this, &diffID]() { UnlockLayer(diffID); });

    // The layer could be unpacked from another blob with the same diff ID while this call was waiting for the lock.
    if (auto err = ValidateUnpackedLayer(layerPath); err.IsNone()) {
        RemoveLayerBlob(path);

        return ErrorEnum::eNone;
    }

    // RemoveLayer keeps the layer of the item being installed, as the item references it, so it is removed here.
    auto [size, err] = RemoveLayerFiles(layerPath);

    // The size of the files removed before a failure is freed as well.
    if (size > 0) {
        GetSpaceAllocator().FreeSpace(size);
    }

    if (!err.IsNone()) {
        return err;
    }

    if (err = InstallUnpackedLayer(path, descriptor.mMediaType, layerPath); !err.IsNone()) {
        return err;
    }

    RemoveLayerBlob(path);

    return ErrorEnum::eNone;
}

RetWithError<size_t> ImageManager::RemoveLayer(
    const String& path, const oci::ContentDescriptor& descriptor, const String& diffID)
{
    (void)path;
    (void)descriptor;

    if (diffID.IsEmpty()) {
        return {0, ErrorEnum::eNone};
    }

    StaticString<cFilePathLen> layerPath;

    if (auto err = CreateLayerPath(diffID, layerPath); !err.IsNone()) {
        return {0, err};
    }

    // It may be called from the item remover, so it must not wait: a layer being processed is in use.
    if (!TryLockLayer(diffID)) {
        return {0, ErrorEnum::eNone};
    }

    auto unlockLayer = DeferRelease([this, &diffID]() { UnlockLayer(diffID); });

    auto [used, err] = mStorage->IsLayerUsed(diffID);
    if (!err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (used) {
        return {0, ErrorEnum::eNone};
    }

    size_t size = 0;

    if (Tie(size, err) = RemoveLayerFiles(layerPath); !err.IsNone()) {
        // It may be called from the item remover under the space allocator lock, so it must not free the space: the
        // size of the partially removed files is dropped as the common image manager does on error.
        return {0, err};
    }

    return {size, ErrorEnum::eNone};
}

RetWithError<size_t> ImageManager::RemoveOrphanLayers()
{
    LOG_DBG() << "Remove orphan layers";

    size_t freedSize = 0;
    Error  firstErr;

    // The layer which is failed to remove is logged and removed in the next period, the first error is returned.
    const auto removeLayer = [this, &firstErr, &freedSize](const String& path, const String& diffID) {
        auto [size, err] = RemoveOrphanLayer(path, diffID);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to remove orphan layer" << Log::Field("path", path) << Log::Field(err);

            if (firstErr.IsNone()) {
                firstErr = err;
            }
        }

        freedSize += size;
    };

    for (auto algIt = fs::DirIterator(mLayersPath); algIt.Next();) {
        const auto algPath = fs::JoinPath(mLayersPath, algIt->mPath);

        // The layers folder contains only the algorithm folders.
        if (!algIt->mIsDir) {
            removeLayer(algPath, "");

            continue;
        }

        for (auto layerIt = fs::DirIterator(algPath); layerIt.Next();) {
            StaticString<oci::cDigestLen> diffID;

            // The diff ID is left empty if it doesn't fit, so the layer is removed as invalid.
            if (algIt->mPath.Size() + 1 + layerIt->mPath.Size() <= diffID.MaxSize()) {
                (void)diffID.Append(algIt->mPath).Append(":").Append(layerIt->mPath);
            }

            removeLayer(fs::JoinPath(algPath, layerIt->mPath), diffID);
        }
    }

    return {freedSize, firstErr};
}

Error ImageManager::CreateLayerPath(const String& diffID, String& path) const
{
    StaticString<oci::cDigestLen> algorithm;
    StaticString<oci::cDigestLen> hash;

    if (auto err = ParseDigest(diffID, algorithm, hash); !err.IsNone()) {
        return err;
    }

    if (auto err = path.Assign(fs::JoinPath(mLayersPath, algorithm, hash)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ValidateUnpackedLayer(const String& layerPath) const
{
    LOG_DBG() << "Validate unpacked layer" << Log::Field("path", layerPath);

    const auto unpackedPath = fs::JoinPath(layerPath, cUnpackedLayerFolder);
    const auto digestPath   = fs::JoinPath(layerPath, cDigestFile);

    auto [exists, err] = fs::DirExist(unpackedPath);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!exists) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "unpacked layer not found"));
    }

    // The metadata is written last, so the layer without it is not completely unpacked.
    const StaticString<cFilePathLen> metadataPaths[] = {digestPath, fs::JoinPath(layerPath, cSizeFile)};

    for (const auto& metadataPath : metadataPaths) {
        if (Tie(exists, err) = fs::FileExist(metadataPath); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (!exists) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "layer metadata not found"));
        }
    }

    StaticString<oci::cDigestLen> storedDigest;

    if (err = fs::ReadFileToString(digestPath, storedDigest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<oci::cDigestLen> layerDigest;

    if (Tie(layerDigest, err) = mImageHandler->GetUnpackedLayerDigest(unpackedPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (layerDigest != storedDigest) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidChecksum, "wrong layer checksum"));
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallUnpackedLayer(const String& path, const String& mediaType, const String& layerPath)
{
    LOG_DBG() << "Unpack layer" << Log::Field("path", path) << Log::Field("layerPath", layerPath);

    auto [unpackedSize, sizeErr] = mImageHandler->GetUnpackedLayerSize(path, mediaType);
    if (!sizeErr.IsNone() && !sizeErr.Is(ErrorEnum::eNotSupported)) {
        return AOS_ERROR_WRAP(sizeErr);
    }

    auto [space, err] = GetSpaceAllocator().AllocateSpace(unpackedSize);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = UnpackToLayerPath(path, mediaType, layerPath, unpackedSize, *space);
    if (err.IsNone()) {
        if (err = space->Accept(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    if (auto removeErr = fs::RemoveAll(layerPath); !removeErr.IsNone()) {
        LOG_ERR() << "Failed to remove layer" << Log::Field("path", layerPath) << Log::Field(AOS_ERROR_WRAP(removeErr));

        // The removal may fail in the middle, so the space of the files left stays used until the layer is processed
        // again.
        AcceptRemainingSpace(layerPath, *space);

        return err;
    }

    if (auto releaseErr = space->Release(); !releaseErr.IsNone()) {
        LOG_ERR() << "Failed to release space" << Log::Field("path", layerPath) << Log::Field(releaseErr);
    }

    return err;
}

Error ImageManager::UnpackToLayerPath(const String& path, const String& mediaType, const String& layerPath,
    size_t unpackedSize, spaceallocator::SpaceItf& space)
{
    if (auto err = fs::MakeDirAll(layerPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mImageHandler->UnpackLayer(path, fs::JoinPath(layerPath, cUnpackedLayerFolder), mediaType);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = CreateLayerMetadata(layerPath, unpackedSize); !err.IsNone()) {
        return err;
    }

    // The layer removal frees the size of the layer files, so the accepted space has to match it.
    auto [size, err] = fs::CalculateSize(*mAllocator, layerPath);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = space.Resize(size); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::CreateLayerMetadata(const String& layerPath, size_t unpackedSize)
{
    auto [digest, err] = mImageHandler->GetUnpackedLayerDigest(fs::JoinPath(layerPath, cUnpackedLayerFolder));
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Create layer metadata" << Log::Field("path", layerPath) << Log::Field("digest", digest)
              << Log::Field("size", unpackedSize);

    StaticString<64> sizeStr;

    if (err = sizeStr.Convert(unpackedSize); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = fs::WriteStringToFile(fs::JoinPath(layerPath, cSizeFile), sizeStr, 0600); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = fs::WriteStringToFile(fs::JoinPath(layerPath, cDigestFile), digest, 0600); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<size_t> ImageManager::RemoveLayerFiles(const String& layerPath)
{
    LOG_DBG() << "Remove layer" << Log::Field("path", layerPath);

    // A file in the layers folder is not a layer, but it is removed the same way.
    auto [exists, err] = fs::FileExist(layerPath);
    if (!err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (!exists) {
        if (Tie(exists, err) = fs::DirExist(layerPath); !err.IsNone()) {
            return {0, AOS_ERROR_WRAP(err)};
        }
    }

    if (!exists) {
        return {0, ErrorEnum::eNone};
    }

    size_t size = 0;

    if (Tie(size, err) = fs::CalculateSize(*mAllocator, layerPath); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (err = fs::RemoveAll(layerPath); !err.IsNone()) {
        // The removal may fail in the middle, so the size of the removed files is returned with the error.
        auto [remainingSize, sizeErr] = fs::CalculateSize(*mAllocator, layerPath);
        if (!sizeErr.IsNone() || remainingSize > size) {
            return {0, AOS_ERROR_WRAP(err)};
        }

        return {size - remainingSize, AOS_ERROR_WRAP(err)};
    }

    return {size, ErrorEnum::eNone};
}

void ImageManager::AcceptRemainingSpace(const String& layerPath, spaceallocator::SpaceItf& space)
{
    auto [size, err] = fs::CalculateSize(*mAllocator, layerPath);
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to calculate layer size" << Log::Field("path", layerPath) << Log::Field(err);
    } else if (err = space.Resize(size); !err.IsNone()) {
        LOG_ERR() << "Failed to resize space" << Log::Field("path", layerPath) << Log::Field(AOS_ERROR_WRAP(err));
    }

    if (err = space.Accept(); !err.IsNone()) {
        LOG_ERR() << "Failed to accept space" << Log::Field("path", layerPath) << Log::Field(AOS_ERROR_WRAP(err));
    }
}

void ImageManager::RemoveLayerBlob(const String& path)
{
    LOG_DBG() << "Remove layer blob" << Log::Field("path", path);

    // The blob is not needed once the layer is unpacked. The blob that can't be removed stays with its space accepted
    // until the layer is processed again or the item is removed.
    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to remove layer blob" << Log::Field("path", path) << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    if (!exists) {
        return;
    }

    size_t size = 0;

    if (Tie(size, err) = fs::CalculateSize(*mAllocator, path); !err.IsNone()) {
        LOG_ERR() << "Failed to remove layer blob" << Log::Field("path", path) << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    if (err = fs::RemoveAll(path); !err.IsNone()) {
        LOG_ERR() << "Failed to remove layer blob" << Log::Field("path", path) << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    if (size > 0) {
        GetSpaceAllocator().FreeSpace(size);
    }
}

Error ImageManager::CheckLayerMediaType(const String& mediaType)
{
    if (mediaType != oci::cMediaTypeLayerTar && mediaType != oci::cMediaTypeLayerTarGZip) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "unsupported layer media type"));
    }

    return ErrorEnum::eNone;
}

RetWithError<size_t> ImageManager::RemoveOrphanLayer(const String& layerPath, const String& diffID)
{
    StaticString<oci::cDigestLen> algorithm;
    StaticString<oci::cDigestLen> hash;

    // The file which doesn't match a valid diff ID is not a layer and is removed.
    if (diffID.IsEmpty() || !ParseDigest(diffID, algorithm, hash).IsNone()) {
        LOG_WRN() << "Remove invalid layer" << Log::Field("path", layerPath);

        return RemoveLayerFiles(layerPath);
    }

    // A locked layer is being processed by an item being installed, so it is in use.
    if (!TryLockLayer(diffID)) {
        return {0, ErrorEnum::eNone};
    }

    auto unlockLayer = DeferRelease([this, &diffID]() { UnlockLayer(diffID); });

    auto [used, err] = mStorage->IsLayerUsed(diffID);
    if (!err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    if (used) {
        return {0, ErrorEnum::eNone};
    }

    LOG_DBG() << "Remove orphan layer" << Log::Field("diffID", diffID);

    return RemoveLayerFiles(layerPath);
}

Error ImageManager::LockLayer(const String& diffID)
{
    UniqueLock lock {mLayersMutex};

    if (auto err = mLayersCondVar.Wait(lock,
            [&]() {
                return mNumCancels > 0
                    || (!IsLayerInProgress(diffID) && mLayersInProgress.Size() < cMaxNumConcurrentItems);
            });
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mNumCancels > 0) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eCanceled, "layer lock canceled"));
    }

    if (auto err = mLayersInProgress.PushBack(diffID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

bool ImageManager::TryLockLayer(const String& diffID)
{
    LockGuard lock {mLayersMutex};

    if (IsLayerInProgress(diffID)) {
        return false;
    }

    if (mLayersInProgress.IsFull()) {
        LOG_ERR() << "Layers in progress list is full" << Log::Field("diffID", diffID);

        return false;
    }

    if (auto err = mLayersInProgress.PushBack(diffID); !err.IsNone()) {
        return false;
    }

    return true;
}

void ImageManager::SetLayersCanceled(bool canceled)
{
    LockGuard lock {mLayersMutex};

    // Counted, as Cancel may be called concurrently.
    if (canceled) {
        mNumCancels++;
    } else {
        mNumCancels--;
    }

    if (auto err = mLayersCondVar.NotifyAll(); !err.IsNone()) {
        LOG_ERR() << "Can't notify condition variable" << Log::Field(err);
    }
}

void ImageManager::UnlockLayer(const String& diffID)
{
    LockGuard lock {mLayersMutex};

    (void)mLayersInProgress.RemoveIf([&diffID](const String& layer) { return layer == diffID; });

    if (auto err = mLayersCondVar.NotifyAll(); !err.IsNone()) {
        LOG_ERR() << "Can't notify condition variable" << Log::Field(err);
    }
}

} // namespace aos::sm::imagemanager
