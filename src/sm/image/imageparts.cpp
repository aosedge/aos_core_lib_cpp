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
        return {"", AOS_ERROR_WRAP(err)};
    }

    if (digestList.Size() != 2) {
        return {"", AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
    }

    return FS::JoinPath(digestList[0], digestList[1]);
}

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

RetWithError<ImageParts> GetImagePartsFromManifest(const oci::ImageManifest& manifest)
{
    auto imageConfig = DigestToPath(manifest.mConfig.mDigest);
    if (!imageConfig.mError.IsNone()) {
        return {{}, imageConfig.mError};
    }

    if (!manifest.mAosService.HasValue()) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotFound)};
    }

    auto serviceConfig = DigestToPath(manifest.mAosService->mDigest);
    if (!serviceConfig.mError.IsNone()) {
        return {{}, serviceConfig.mError};
    }

    if (!manifest.mLayers) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotFound)};
    }

    auto serviceFS = DigestToPath(manifest.mLayers[0].mDigest);
    if (!serviceFS.mError.IsNone()) {
        return {{}, serviceFS.mError};
    }

    return image::ImageParts {imageConfig.mValue, serviceConfig.mValue, serviceFS.mValue};
}

} // namespace aos::sm::image
