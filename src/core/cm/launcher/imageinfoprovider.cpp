/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>
#include <core/common/tools/memory.hpp>

#include "imageinfoprovider.hpp"

namespace aos::cm::launcher {

void ImageInfoProvider::Init(imagemanager::ItemInfoProviderItf& itemInfoProvider, oci::OCISpecItf& ociSpec)
{
    mItemInfoProvider = &itemInfoProvider;
    mOCISpec          = &ociSpec;
}

Error ImageInfoProvider::GetImageConfig(const oci::IndexContentDescriptor& imageDescriptor, oci::ImageConfig& config)
{
    auto manifestPath = MakeUnique<StaticString<cFilePathLen>>(&mAllocator);
    auto manifest     = MakeUnique<oci::ImageManifest>(&mAllocator);
    auto configPath   = MakeUnique<StaticString<cFilePathLen>>(&mAllocator);

    if (auto err = mItemInfoProvider->GetBlobPath(imageDescriptor.mDigest, *manifestPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCISpec->LoadImageManifest(*manifestPath, *manifest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mItemInfoProvider->GetBlobPath(manifest->mConfig.mDigest, *configPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCISpec->LoadImageConfig(*configPath, config); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageInfoProvider::GetServiceConfig(
    const oci::IndexContentDescriptor& imageDescriptor, oci::ServiceConfig& serviceConfig)
{
    auto manifestPath = MakeUnique<StaticString<cFilePathLen>>(&mAllocator);
    auto manifest     = MakeUnique<oci::ImageManifest>(&mAllocator);
    auto servicePath  = MakeUnique<StaticString<cFilePathLen>>(&mAllocator);

    if (auto err = mItemInfoProvider->GetBlobPath(imageDescriptor.mDigest, *manifestPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCISpec->LoadImageManifest(*manifestPath, *manifest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!manifest->mAosService.HasValue()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = mItemInfoProvider->GetBlobPath(manifest->mAosService->mDigest, *servicePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCISpec->LoadServiceConfig(*servicePath, serviceConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ImageInfoProvider::GetImageIndex(const String& itemID, const String& version, oci::ImageIndex& imageIndex)
{
    auto indexDigest = MakeUnique<StaticString<oci::cDigestLen>>(&mAllocator);
    auto indexPath   = MakeUnique<StaticString<cFilePathLen>>(&mAllocator);

    if (auto err = mItemInfoProvider->GetIndexDigest(itemID, version, *indexDigest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mItemInfoProvider->GetBlobPath(*indexDigest, *indexPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCISpec->LoadImageIndex(*indexPath, imageIndex); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::launcher
