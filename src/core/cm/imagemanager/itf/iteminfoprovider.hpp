/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_ITEMINFOPROVIDER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_ITEMINFOPROVIDER_HPP_

#include <core/common/types/unitstatus.hpp>

namespace aos::cm::imagemanager {

/**
 * Interface to provide update item info.
 */
class ItemInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ItemInfoProviderItf() = default;

    /**
     * Returns update item index digest.
     *
     * @param itemID update item ID.
     * @param version update item version.
     * @param[out] digest result item digest.
     * @return Error.
     */
    virtual Error GetIndexDigest(const String& itemID, const String& version, String& digest) const = 0;

    /**
     * Returns blob path by its digest.
     *
     * @param digest blob digest.
     * @param[out] path result blob path.
     * @return Error.
     */
    virtual Error GetBlobPath(const String& digest, String& path) const = 0;

    /**
     * Returns blob URL by its digest.
     *
     * @param digest blob digest.
     * @param[out] url result blob URL.
     * @return Error.
     */
    virtual Error GetBlobURL(const String& digest, String& url) const = 0;

    /**
     * Returns item current version.
     *
     * @param itemID update item ID.
     * @param[out] version result item version.
     * @return Error.
     */
    virtual Error GetItemCurrentVersion(const String& itemID, String& version) const = 0;
};

} // namespace aos::cm::imagemanager

#endif
