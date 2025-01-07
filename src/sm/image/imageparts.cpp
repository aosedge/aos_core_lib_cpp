/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/image/imageparts.hpp"

namespace aos::sm::image {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

RetWithError<StaticString<cFilePathLen>> DigestToPath(const String& digest)
{
    StaticArray<const StaticString<oci::cMaxDigestLen>, 2> digestList;

    if (auto err = digest.Split(digestList, ':'); !err.IsNone()) {
        return {{}, AOS_ERROR_WRAP(err)};
    }

    if (digestList.Size() != 2) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
    }

    return FS::JoinPath(digestList[0], digestList[1]);
}

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error GetImagePartsFromManifest(const oci::ImageManifest& manifest, ImageParts& imageParts)
{
    Error err;

    if (Tie(imageParts.mImageConfigPath, err) = DigestToPath(manifest.mConfig.mDigest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (manifest.mAosService.HasValue()) {
        if (Tie(imageParts.mServiceConfigPath, err) = DigestToPath(manifest.mAosService->mDigest); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (manifest.mLayers.IsEmpty()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (Tie(imageParts.mServiceFSPath, err) = DigestToPath(manifest.mLayers[0].mDigest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (auto it = manifest.mLayers.begin() + 1; it != manifest.mLayers.end(); ++it) {
        if (err = imageParts.mLayerDigests.PushBack(it->mDigest); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::image
