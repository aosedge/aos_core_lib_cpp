/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IMAGEMANAGER_ITF_BLOBINFOPROVIDER_HPP_
#define AOS_CORE_COMMON_IMAGEMANAGER_ITF_BLOBINFOPROVIDER_HPP_

#include <core/common/types/blobs.hpp>

namespace aos::common::imagemanager {

/**
 * Interface that provides download blob info.
 */
class BlobInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~BlobInfoProviderItf() = default;

    /**
     * Returns blob info.
     *
     * @param digest blob digest.
     * @param[out] blobInfo blob info.
     * @return Error.
     */
    virtual Error GetBlobInfo(const String& digest, BlobInfo& blobInfo) = 0;
};

} // namespace aos::common::imagemanager

#endif
