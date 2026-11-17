/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "itemconfigloader.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void ItemConfigLoader::Init(
    AllocatorItf& allocator, common::imagemanager::ItemInfoProviderItf& itemInfoProvider, oci::OCISpecItf& ociSpec)
{
    mAllocator        = &allocator;
    mItemInfoProvider = &itemInfoProvider;
    mOCISpec          = &ociSpec;
}

Error ItemConfigLoader::Load(const String& manifestDigest, oci::ItemConfig& itemConfig, oci::ImageConfig& imageConfig)
{
    auto path = MakeUnique<StaticString<cFilePathLen>>(mAllocator);
    if (!path) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mItemInfoProvider->GetBlobPath(manifestDigest, *path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto manifest = MakeUnique<oci::ImageManifest>(mAllocator);
    if (!manifest) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mOCISpec->LoadImageManifest(*path, *manifest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!manifest->mItemConfig.HasValue()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound, "item config not found in manifest"));
    }

    if (auto err = mItemInfoProvider->GetBlobPath(manifest->mItemConfig->mDigest, *path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCISpec->LoadItemConfig(*path, itemConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mItemInfoProvider->GetBlobPath(manifest->mConfig.mDigest, *path); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mOCISpec->LoadImageConfig(*path, imageConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::launcher
