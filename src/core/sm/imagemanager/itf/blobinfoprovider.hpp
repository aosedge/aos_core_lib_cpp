/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_ITF_BLOBINFOPROVIDER_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_ITF_BLOBINFOPROVIDER_HPP_

#include <core/common/ocispec/itf/imagespec.hpp>
#include <core/common/types/common.hpp>

namespace aos::sm::imagemanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Interface that provides blob info.
 */
class BlobInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~BlobInfoProviderItf() = default;

    /**
     * Returns blobs info.
     *
     * @param digests list of blob digests.
     * @param[out] urls blobs URLs.
     * @return Error.
     */
    virtual Error GetBlobsInfo(const Array<StaticString<oci::cDigestLen>>& digests, Array<StaticString<cURLLen>>& urls)
        = 0;
};

/** @}*/

} // namespace aos::sm::imagemanager

#endif
