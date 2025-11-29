/**
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_BLOBS_HPP_
#define AOS_CORE_COMMON_TYPES_BLOBS_HPP_

#include <core/common/crypto/cryptohelper.hpp>
#include <core/common/ocispec/itf/imagespec.hpp>

#include "common.hpp"

namespace aos {

/**
 * Blob info.
 */
struct BlobInfo {
    StaticString<oci::cDigestLen>                   mDigest;
    StaticArray<StaticString<cURLLen>, cMaxNumURLs> mURLs;
    StaticArray<uint8_t, crypto::cSHA256Size>       mSHA256;
    size_t                                          mSize {};
    crypto::DecryptInfo                             mDecryptInfo;
    crypto::SignInfo                                mSignInfo;

    /**
     * Compares blob info.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const BlobInfo& rhs) const
    {
        return mURLs == rhs.mURLs && mSHA256 == rhs.mSHA256 && mSize == rhs.mSize && mDecryptInfo == rhs.mDecryptInfo
            && mSignInfo == rhs.mSignInfo;
    }

    /**
     * Compares blob info.
     */
    bool operator!=(const BlobInfo& rhs) const { return !operator==(rhs); }
};

using BlobInfoArray = StaticArray<BlobInfo, cMaxNumBlobs>;

/**
 * Blob URLs request.
 */
struct BlobURLsRequest {
    StaticArray<StaticString<oci::cDigestLen>, cMaxNumBlobs> mDigests;

    /**
     * Compares blob URLs request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const BlobURLsRequest& rhs) const { return mDigests == rhs.mDigests; }

    /**
     * Compares blob URLs request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const BlobURLsRequest& rhs) const { return !operator==(rhs); }
};

struct BlobURLsInfo {
    BlobInfoArray mItems;

    /**
     * Compares blob URLs info.
     *
     * @param rhs blob URLs info to compare with.
     * @return bool.
     */
    bool operator==(const BlobURLsInfo& rhs) const { return mItems == rhs.mItems; }

    /**
     * Compares blob URLs info.
     *
     * @param rhs blob URLs info to compare with.
     * @return bool.
     */
    bool operator!=(const BlobURLsInfo& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
