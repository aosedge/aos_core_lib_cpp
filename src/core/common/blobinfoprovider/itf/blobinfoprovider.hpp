/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_BLOBINFOPROVIDER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_BLOBINFOPROVIDER_HPP_

#include <core/common/types/blobs.hpp>

namespace aos::blobinfoprovider {

/**
 * Interface that provides blob info.
 */
class ProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ProviderItf() = default;

    /**
     * Returns blobs info.
     *
     * @param digests list of blob digests.
     * @param[out] blobsInfos blobs infos.
     * @return Error.
     */
    virtual Error GetBlobsInfos(const Array<StaticString<oci::cDigestLen>>& digests, Array<BlobInfo>& blobsInfos) = 0;
};

} // namespace aos::blobinfoprovider

#endif
