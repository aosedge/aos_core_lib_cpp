/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "imagemanager.hpp"

namespace aos::sm::imagemanager {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

Error SplitDigest(const String& digest, String& alg, String& hash)
{
    StaticArray<StaticString<oci::cDigestLen>, 2> digestList;

    if (auto err = digest.Split(digestList, ':'); !err.IsNone()) {
        return err;
    }

    if (digestList.Size() != 2) {
        return ErrorEnum::eInvalidArgument;
    }

    alg  = digestList[0];
    hash = digestList[1];

    return ErrorEnum::eNone;
}

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ImageManager::Init(const Config& config, BlobInfoProviderItf& blobInfoProvider,
    spaceallocator::SpaceAllocatorItf& spaceAllocator, downloader::DownloaderItf& downloader,
    fs::FileInfoProviderItf& fileInfoProvider, oci::OCISpecItf& ociSpec, ImageHandlerItf& imageHandler)
{
    LOG_DBG() << "Init image manager";

    mConfig           = config;
    mBlobInfoProvider = &blobInfoProvider;
    mSpaceAllocator   = &spaceAllocator;
    mDownloader       = &downloader;
    mFileInfoProvider = &fileInfoProvider;
    mOCISpec          = &ociSpec;
    mImageHandler     = &imageHandler;

    LOG_DBG() << "Config" << Log::Field("imagePath", mConfig.mImagePath) << Log::Field("partLimit", mConfig.mPartLimit)
              << Log::Field("updateItemTTL", mConfig.mUpdateItemTTL)
              << Log::Field("removeOutdatedPeriod", mConfig.mRemoveOutdatedPeriod);

    if (auto err = fs::MakeDirAll(fs::JoinPath(mConfig.mImagePath, cBlobsFolder)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::MakeDirAll(fs::JoinPath(mConfig.mImagePath, cLayersFolder)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetAllInstalledItems(Array<UpdateItemStatus>& statuses) const
{
    LOG_DBG() << "Get all installed items";

    for (const auto& status : mInstalledItems) {
        if (auto err = statuses.PushBack(status); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallUpdateItem(const UpdateItemInfo& itemInfo)
{
    LOG_INF() << "Install item" << Log::Field("itemID", itemInfo.mID) << Log::Field("version", itemInfo.mVersion)
              << Log::Field("type", itemInfo.mType) << Log::Field("manifestDigest", itemInfo.mManifestDigest);

    oci::ContentDescriptor manifestDescriptor {"", itemInfo.mManifestDigest, 0};

    LOG_DBG() << "Install manifest blob" << Log::Field("digest", itemInfo.mManifestDigest);

    if (auto err = InstallBlob(manifestDescriptor); !err.IsNone()) {
        return err;
    }

    auto manifest = MakeUnique<oci::ImageManifest>(&mAllocator);

    StaticString<cFilePathLen> path;

    if (auto err = CreateBlobPath(itemInfo.mManifestDigest, path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCISpec->LoadImageManifest(path, *manifest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (itemInfo.mType == UpdateItemTypeEnum::eService) {
        LOG_DBG() << "Install image config blob" << Log::Field("digest", manifest->mConfig.mDigest);

        if (auto err = InstallBlob(manifest->mConfig); !err.IsNone()) {
            return err;
        }

        auto config = MakeUnique<oci::ImageConfig>(&mAllocator);

        if (auto err = CreateBlobPath(manifest->mConfig.mDigest, path); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = mOCISpec->LoadImageConfig(path, *config); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (manifest->mAosService.HasValue()) {
            LOG_DBG() << "Install service config blob" << Log::Field("digest", manifest->mAosService->mDigest);

            if (auto err = InstallBlob(*manifest->mAosService); !err.IsNone()) {
                return err;
            }
        }

        for (size_t i = 0; i < manifest->mLayers.Size(); ++i) {
            const auto& layer = manifest->mLayers[i];

            if (i >= config->mRootfs.mDiffIDs.Size()) {
                return AOS_ERROR_WRAP(Error(ErrorEnum::eOutOfRange, "diff IDs size is less than layers size"));
            }

            LOG_DBG() << "Install layer blob" << Log::Field("digest", layer.mDigest)
                      << Log::Field("diffDigest", config->mRootfs.mDiffIDs[i]);

            if (auto err = InstallLayer(layer, config->mRootfs.mDiffIDs[i]); !err.IsNone()) {
                return err;
            }
        }
    } else {
        for (const auto& layer : manifest->mLayers) {
            LOG_DBG() << "Install layer blob" << Log::Field("digest", layer.mDigest);

            if (auto err = InstallBlob(layer); !err.IsNone()) {
                return err;
            }
        }
    }

    auto it = mInstalledItems.FindIf([&itemInfo](const UpdateItemStatus& status) {
        return status.mID == itemInfo.mID && status.mVersion == itemInfo.mVersion;
    });
    if (it != mInstalledItems.end()) {
        *it = UpdateItemStatus {itemInfo.mID, itemInfo.mType, itemInfo.mVersion, ItemStateEnum::eInstalled};

        return ErrorEnum::eNone;
    }

    if (auto err = mInstalledItems.PushBack(
            UpdateItemStatus {itemInfo.mID, itemInfo.mType, itemInfo.mVersion, ItemStateEnum::eInstalled});
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::RemoveUpdateItem(const String& itemID, const String& version)
{
    LOG_INF() << "Remove item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    auto it = mInstalledItems.FindIf([&itemID, &version](const UpdateItemStatus& status) {
        return status.mID == itemID && status.mVersion == version;
    });
    if (it != mInstalledItems.end()) {
        mInstalledItems.Erase(it);
    } else {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetBlobPath(const String& digest, String& path) const
{
    if (auto err = CreateBlobPath(digest, path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Get blob path" << Log::Field("digest", digest) << Log::Field("path", path);

    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!exists) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetLayerPath(const String& digest, String& path) const
{
    if (auto err = CreateLayerPath(digest, path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    path = fs::JoinPath(path, cUnpackedLayerFolder);

    LOG_DBG() << "Get layer path" << Log::Field("digest", digest) << Log::Field("path", path);

    auto [exists, err] = fs::DirExist(path);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!exists) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

RetWithError<size_t> ImageManager::RemoveItem(const String& id)
{
    LOG_DBG() << "Remove item" << Log::Field("id", id);

    return 0;
}

Error ImageManager::CreateBlobPath(const String& digest, String& path) const
{
    StaticString<oci::cDigestLen> alg;
    StaticString<oci::cDigestLen> hash;

    if (auto err = SplitDigest(digest, alg, hash); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    path = fs::JoinPath(mConfig.mImagePath, cBlobsFolder, alg, hash);

    return ErrorEnum::eNone;
}

Error ImageManager::CreateLayerPath(const String& digest, String& path) const
{
    StaticString<oci::cDigestLen> alg;
    StaticString<oci::cDigestLen> hash;

    if (auto err = SplitDigest(digest, alg, hash); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    path = fs::JoinPath(mConfig.mImagePath, cLayersFolder, alg, hash);

    return ErrorEnum::eNone;
}

Error ImageManager::ValidateBlob(const String& path, const String& digest) const
{
    LOG_DBG() << "Validate blob" << Log::Field("digest", digest);

    StaticString<oci::cDigestLen> alg;
    StaticString<oci::cDigestLen> hash;

    if (auto err = SplitDigest(digest, alg, hash); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (alg.Compare(crypto::Hash(crypto::HashEnum::eSHA256).ToString(), String::CaseSensitivity::CaseInsensitive)
        != 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotSupported);
    }

    fs::FileInfo fileInfo;

    if (auto err = mFileInfoProvider->GetFileInfo(path, fileInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticArray<uint8_t, crypto::cSHA256Size> hashBytes;

    if (auto err = hash.HexToByteArray(hashBytes); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (fileInfo.mSHA256 != hashBytes) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidChecksum);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::ValidateLayer(const String& path, const String& diffDigest) const
{
    LOG_DBG() << "Validate layer" << Log::Field("path", path) << Log::Field("diffDigest", diffDigest);

    auto [size, err] = fs::CalculateSize(path);

    if (size > oci::cDigestLen) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "not link to unpacked layer"));
    }

    StaticString<oci::cDigestLen> digest;

    if (err = fs::ReadFileToString(path, digest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (digest != diffDigest) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidChecksum, "wrong diff digest"));
    }

    StaticString<cFilePathLen> dstPath;

    if (err = CreateLayerPath(diffDigest, dstPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = fs::ReadFileToString(fs::JoinPath(dstPath, cDigestFile), digest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<oci::cDigestLen> layerDigest;

    Tie(layerDigest, err) = mImageHandler->GetUnpackedLayerDigest(fs::JoinPath(dstPath, cUnpackedLayerFolder));
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Layer checksum" << Log::Field("path", path) << Log::Field("layerDigest", layerDigest);

    if (layerDigest != digest) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidChecksum, "wrong layer checksum"));
    }

    return ErrorEnum::eNone;
}

Error ImageManager::DownloadBlob(const String& path, const String& digest, size_t size)
{
    Error                               err;
    UniquePtr<spaceallocator::SpaceItf> space;
    StaticString<cURLLen>               url;

    auto releaseSpace = DeferRelease(&err, [&](const Error* err) {
        if (!err->IsNone()) {
            LOG_ERR() << "Failed to download blob" << Log::Field("digest", digest) << Log::Field(*err);
        }

        ReleaseSpace(path, space.Get(), *err);
    });

    if (err = GetBlobURL(digest, url); !err.IsNone()) {
        return err;
    }

    if (size) {
        Tie(space, err) = mSpaceAllocator->AllocateSpace(size);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    LOG_DBG() << "Download blob" << Log::Field("digest", digest) << Log::Field("size", size) << Log::Field("url", url);

    if (err = mDownloader->Download(digest, url, path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallBlob(const oci::ContentDescriptor& descriptor)
{
    LOG_DBG() << "Install blob" << Log::Field("digest", descriptor.mDigest) << Log::Field("size", descriptor.mSize);

    StaticString<cFilePathLen> path;

    if (auto err = CreateBlobPath(descriptor.mDigest, path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Blob path" << Log::Field("digest", descriptor.mDigest) << Log::Field("path", path);

    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (exists) {
        if (err = ValidateBlob(path, descriptor.mDigest); err.IsNone()) {
            return ErrorEnum::eNone;
        }

        if (err = fs::RemoveAll(path); !err.IsNone()) {
            LOG_ERR() << "Failed to remove blob" << Log::Field("digest", descriptor.mDigest) << Log::Field(err);
        }
    }

    StaticString<cFilePathLen> parentPath;

    if (err = fs::ParentPath(path, parentPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = fs::MakeDirAll(parentPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = DownloadBlob(path, descriptor.mDigest, descriptor.mSize); !err.IsNone()) {
        return err;
    }

    if (err = ValidateBlob(path, descriptor.mDigest); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::CreateLayerMetadata(const String& path, size_t size, spaceallocator::SpaceItf* space)
{
    auto [digest, err] = mImageHandler->GetUnpackedLayerDigest(fs::JoinPath(path, cUnpackedLayerFolder));
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Create layer metadata" << Log::Field("path", path) << Log::Field("digest", digest)
              << Log::Field("size", size);

    if (err = space->Resize(space->Size() + digest.Size()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = fs::WriteStringToFile(fs::JoinPath(path, cDigestFile), digest, 0600); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticString<64> sizeStr;

    if (err = sizeStr.Convert(size); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = space->Resize(space->Size() + sizeStr.Size()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = fs::WriteStringToFile(fs::JoinPath(path, cSizeFile), sizeStr, 0600); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::UnpackLayer(const String& path, const oci::ContentDescriptor& descriptor, const String& diffDigest)
{
    LOG_DBG() << "Unpack layer" << Log::Field("diffDigest", diffDigest) << Log::Field("path", path);

    StaticString<cFilePathLen> dstPath;

    if (auto err = CreateLayerPath(diffDigest, dstPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Layer destination" << Log::Field("diffDigest", diffDigest) << Log::Field("path", dstPath);

    if (auto err = fs::ClearDir(dstPath); !err.IsNone()) {
        LOG_ERR() << "Failed to remove layer" << Log::Field("diffDigest", diffDigest) << Log::Field(err);
    }

    auto [size, err] = mImageHandler->GetUnpackedLayerSize(path, descriptor.mMediaType);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotSupported)) {
        LOG_ERR() << "Failed to get unpacked size" << Log::Field("path", path) << Log::Field(err);
    }

    UniquePtr<spaceallocator::SpaceItf> space;

    auto releaseSpace = DeferRelease(&err, [&](const Error* err) {
        if (!err->IsNone()) {
            LOG_ERR() << "Failed to unpack layer" << Log::Field("diffDigest", diffDigest) << Log::Field(*err);
        }

        ReleaseSpace(dstPath, space.Get(), *err);
    });

    if (size) {
        Tie(space, err) = mSpaceAllocator->AllocateSpace(size);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (err = mImageHandler->UnpackLayer(path, fs::JoinPath(dstPath, cUnpackedLayerFolder), descriptor.mMediaType);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = CreateLayerMetadata(dstPath, size, space.Get()); !err.IsNone()) {
        return err;
    }

    LOG_DBG() << "Remove packed layer" << Log::Field("path", path);

    if (err = fs::RemoveAll(path); !err.IsNone()) {
        LOG_ERR() << "Failed to remove packed layer" << Log::Field("path", path) << Log::Field(err);
    } else {
        mSpaceAllocator->FreeSpace(descriptor.mSize);
    }

    if (err = space->Resize(space->Size() + diffDigest.Size()); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Create diff digest file" << Log::Field("diffDigest", diffDigest) << Log::Field("path", path);

    if (err = fs::WriteStringToFile(path, diffDigest, 0600); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageManager::InstallLayer(const oci::ContentDescriptor& descriptor, const String& diffDigest)
{
    LOG_DBG() << "Install layer" << Log::Field("digest", descriptor.mDigest);

    StaticString<cFilePathLen> path;

    if (auto err = CreateBlobPath(descriptor.mDigest, path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto [exists, err] = fs::FileExist(path);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (exists) {
        if (err = ValidateLayer(path, diffDigest); !err.IsNone()) {
            LOG_WRN() << "Layer validation failed" << Log::Field("path", path) << Log::Field("diffDigest", diffDigest)
                      << Log::Field(err);
        } else {
            return ErrorEnum::eNone;
        }
    }

    if (err = InstallBlob(descriptor); !err.IsNone()) {
        return err;
    }

    if (err = UnpackLayer(path, descriptor, diffDigest); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ImageManager::GetBlobURL(const String& digest, String& url) const
{
    StaticArray<StaticString<oci::cDigestLen>, 1> digests;
    StaticArray<StaticString<cURLLen>, 1>         urls;

    if (auto err = digests.EmplaceBack(digest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mBlobInfoProvider->GetBlobsInfo(digests, urls); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (urls.IsEmpty()) {
        return Error(ErrorEnum::eNotFound, "blob URL not found");
    }

    url = urls[0];

    return ErrorEnum::eNone;
}

// cppcheck-suppress passedByValue
void ImageManager::ReleaseSpace(const String& path, spaceallocator::SpaceItf* space, Error err)
{
    if (!err.IsNone()) {
        if (auto removeErr = fs::RemoveAll(path); !removeErr.IsNone()) {
            LOG_ERR() << "Failed to remove path" << Log::Field("path", path) << Log::Field(removeErr);
        }
    }

    if (space) {
        if (!err.IsNone()) {
            space->Release();
        } else {
            space->Accept();
        }
    }
}

} // namespace aos::sm::imagemanager
