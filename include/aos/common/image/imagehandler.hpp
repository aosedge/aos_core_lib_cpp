/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IMAGEHANDLER_HPP_
#define AOS_IMAGEHANDLER_HPP_

#include "aos/common/ocispec/ocispec.hpp"
#include "aos/common/spaceallocator/spaceallocator.hpp"
#include "aos/common/tools/string.hpp"
#include "aos/common/types.hpp"

namespace aos::imagehandler {

/**
 * Image handler interface.
 */
class ImageHandlerItf {
public:
    /**
     * Installs layer from the provided archive.
     *
     * @param archivePath archive path.
     * @param installBasePath installation base path.
     * @param layer layer info.
     * @param space[out] installed layer space.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> InstallLayer(const String& archivePath,
        const String& installBasePath, const LayerInfo& layer, UniquePtr<spaceallocator::SpaceItf>& space) const
        = 0;

    /**
     * Installs service from the provided archive.
     *
     * @param archivePath archive path.
     * @param installBasePath installation base path.
     * @param service service info.
     * @param space[out] installed service space.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> InstallService(const String& archivePath,
        const String& installBasePath, const ServiceInfo& service, UniquePtr<spaceallocator::SpaceItf>& space) const
        = 0;

    /**
     * Validates service.
     *
     * @param path service path.
     * @return Error.
     */
    virtual Error ValidateService(const String& path) const = 0;

    /**
     * Calculates digest for the given path or file.
     *
     * @param path root folder or file.
     * @return RetWithError<StaticString<cMaxDigestLen>>.
     */
    virtual RetWithError<StaticString<oci::cMaxDigestLen>> CalculateDigest(const String& path) const = 0;

    /**
     * Destructor.
     */
    virtual ~ImageHandlerItf() = default;
};

} // namespace aos::imagehandler

#endif
