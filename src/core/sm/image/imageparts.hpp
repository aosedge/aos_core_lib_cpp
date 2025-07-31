/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGE_IMAGEPARTS_HPP_
#define AOS_CORE_SM_IMAGE_IMAGEPARTS_HPP_

#include <core/common/ocispec/ocispec.hpp>
#include <core/common/types/types.hpp>

namespace aos::sm::image {

/**
 * Image parts.
 */
struct ImageParts {
    /**
     * Image config path.
     */
    StaticString<cFilePathLen> mImageConfigPath;

    /**
     * Service config path.
     */
    StaticString<cFilePathLen> mServiceConfigPath;

    /**
     * Service root FS path.
     */
    StaticString<cFilePathLen> mServiceFSPath;

    /**
     * Layer digests.
     */
    StaticArray<StaticString<cLayerDigestLen>, cMaxNumLayers> mLayerDigests;
};

/**
 * Returns image parts.
 *
 * @param manifest image manifest.
 * @param imageParts[out] image parts.
 * @return Error.
 */
Error GetImagePartsFromManifest(const oci::ImageManifest& manifest, ImageParts& imageParts);

} // namespace aos::sm::image

#endif
