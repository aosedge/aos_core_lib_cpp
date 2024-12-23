/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IMAGEPARTS_HPP_
#define AOS_IMAGEPARTS_HPP_

#include "aos/common/ocispec/ocispec.hpp"
#include "aos/common/types.hpp"

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
};

/**
 * Returns image parts.
 *
 * @param manifest image manifest.
 * @return RetWithError<ImageParts>.
 */
RetWithError<ImageParts> GetImagePartsFromManifest(const oci::ImageManifest& manifest);

} // namespace aos::sm::image

#endif
