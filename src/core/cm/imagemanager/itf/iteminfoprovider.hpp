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
     * @return RetWithError<StaticString<oci::cDigestLen>>.
     */
    virtual RetWithError<StaticString<oci::cDigestLen>> GetIndexDigest(
        const String& itemID, const String& version) const
        = 0;

    /**
     * Returns blob path by its digest.
     *
     * @param digest blob digest.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> GetBlobPath(const String& digest) const = 0;

    /**
     * Returns blob URL by its digest.
     *
     * @param digest blob digest.
     * @return RetWithError<StaticString<cURLLen>>.
     */
    virtual RetWithError<StaticString<cURLLen>> GetBlobURL(const String& digest) const = 0;

    /**
     * Returns item current version.
     *
     * @param itemID update item ID.
     * @return RetWithError<StaticString<cVersionLen>>.
     */
    virtual RetWithError<StaticString<cVersionLen>> GetItemCurrentVersion(const String& itemID) const = 0;
};

} // namespace aos::cm::imagemanager

#endif
