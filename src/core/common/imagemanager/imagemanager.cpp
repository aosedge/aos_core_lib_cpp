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

    return ErrorEnum::eNone;
}

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
    (void)statuses;

    LOG_DBG() << "Get items statuses";

    return ErrorEnum::eNotSupported;
}

Error ImageManager::SubscribeListener(ItemStatusListenerItf& listener)
{
    (void)listener;

    LOG_DBG() << "Subscribe listener";

    return ErrorEnum::eNotSupported;
}

Error ImageManager::UnsubscribeListener(ItemStatusListenerItf& listener)
{
    (void)listener;

    LOG_DBG() << "Unsubscribe listener";

    return ErrorEnum::eNotSupported;
}

Error ImageManager::GetBlobPath(const String& digest, String& path) const
{
    (void)path;

    LOG_DBG() << "Get blob path" << Log::Field("digest", digest);

    return ErrorEnum::eNotSupported;
}

/***********************************************************************************************************************
 * Protected
 **********************************************************************************************************************/

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

Error ImageManager::ValidateBlob(const String& digest, const String& path)
{
    LOG_DBG() << "Validate blob" << Log::Field("digest", digest) << Log::Field("path", path);

    return ErrorEnum::eNotSupported;
}

Error ImageManager::InstallLayer(const oci::ContentDescriptor& descriptor, const String& diffDigest, const String& path)
{
    LOG_DBG() << "Install layer" << Log::Field("digest", descriptor.mDigest) << Log::Field("diffDigest", diffDigest)
              << Log::Field("path", path);

    return ErrorEnum::eNotSupported;
}

RetWithError<size_t> ImageManager::RemoveLayer(
    const String& digest, const String& mediaType, const String& diffID, const String& path)
{
    LOG_DBG() << "Remove layer" << Log::Field("digest", digest) << Log::Field("mediaType", mediaType)
              << Log::Field("diffID", diffID) << Log::Field("path", path);

    return {0, ErrorEnum::eNotSupported};
}

Error ImageManager::ValidateLayer(
    const oci::ContentDescriptor& descriptor, const String& diffDigest, const String& path)
{
    LOG_DBG() << "Validate layer" << Log::Field("digest", descriptor.mDigest) << Log::Field("diffDigest", diffDigest)
              << Log::Field("path", path);

    return ErrorEnum::eNotSupported;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

RetWithError<size_t> ImageManager::RemoveOutdatedItem(const String& itemID, const String& version)
{
    LOG_DBG() << "Remove outdated item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    return {0, ErrorEnum::eNotSupported};
}

} // namespace aos::common::imagemanager
