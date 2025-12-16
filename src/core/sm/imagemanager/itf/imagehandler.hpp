/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_ITF_IMAGEHANDLER_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_ITF_IMAGEHANDLER_HPP_

#include <core/common/ocispec/itf/ocispec.hpp>
#include <core/common/tools/string.hpp>

namespace aos::sm::imagemanager {

/**
 * Image handler interface.
 */
class ImageHandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageHandlerItf() = default;

    /**
     * Unpacks layer to the destination path.
     *
     * @param src source layer path.
     * @param dst destination path.
     * @param mediaType layer media type.
     * @return Error.
     */
    virtual Error UnpackLayer(const String& src, const String& dst, const String& mediaType) = 0;

    /**
     * Returns unpacked layer size.
     *
     * @param path packed layer path.
     * @param mediaType layer media type.
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> GetUnpackedLayerSize(const String& path, const String& mediaType) const = 0;

    /**
     * Returns unpacked layer digest.
     *
     * @param path unpacked layer path.
     * @return RetWithError<StaticString<oci::cDigestLen>>.
     */
    virtual RetWithError<StaticString<oci::cDigestLen>> GetUnpackedLayerDigest(const String& path) const = 0;
};

} // namespace aos::sm::imagemanager

#endif
