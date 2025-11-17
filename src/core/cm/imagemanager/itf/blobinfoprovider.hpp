/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_BLOBINFOPROVIDER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_BLOBINFOPROVIDER_HPP_

#include <core/common/types/desiredstatus.hpp>

namespace aos::cm::imagemanager {

/** @addtogroup cm Communication Manager
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
     * @param[out] blobsInfo blobs info.
     * @return Error.
     */
    virtual Error GetBlobsInfo(const Array<StaticString<oci::cDigestLen>>& digests, Array<BlobInfo>& blobsInfo) = 0;
};

/** @}*/

} // namespace aos::cm::imagemanager

#endif
