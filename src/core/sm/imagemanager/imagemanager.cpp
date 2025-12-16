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

Error ImageManager::Init(const Config& config)
{
    LOG_DBG() << "Init image manager";

    mConfig = config;

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
    (void)statuses;

    LOG_DBG() << "Get all installed items";

    return ErrorEnum::eNone;
}

Error ImageManager::InstallUpdateItem(const UpdateItemInfo& itemInfo)
{
    LOG_DBG() << "Install item" << Log::Field("itemID", itemInfo.mID) << Log::Field("version", itemInfo.mVersion)
              << Log::Field("type", itemInfo.mType) << Log::Field("manifestDigest", itemInfo.mManifestDigest);

    return ErrorEnum::eNone;
}

Error ImageManager::RemoveUpdateItem(const String& itemID, const String& version)
{
    LOG_DBG() << "Remove item" << Log::Field("itemID", itemID) << Log::Field("version", version);

    return ErrorEnum::eNone;
}

Error ImageManager::GetBlobPath(const String& digest, String& path) const
{
    (void)path;

    LOG_DBG() << "Get blob path" << Log::Field("digest", digest);

    return ErrorEnum::eNone;
}

Error ImageManager::GetLayerPath(const String& digest, String& path) const
{
    (void)path;

    LOG_DBG() << "Get layer path" << Log::Field("digest", digest);

    return ErrorEnum::eNone;
}

} // namespace aos::sm::imagemanager
