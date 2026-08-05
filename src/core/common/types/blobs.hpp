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
    Optional<crypto::DecryptInfo>                   mDecryptInfo;
    Optional<crypto::SignInfo>                      mSignInfo;

    /**
     * Compares blob info.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator==(const BlobInfo& lhs, const BlobInfo& rhs)
    {
        return lhs.mURLs == rhs.mURLs && lhs.mSHA256 == rhs.mSHA256 && lhs.mSize == rhs.mSize
            && lhs.mDecryptInfo == rhs.mDecryptInfo && lhs.mSignInfo == rhs.mSignInfo;
    };

    /**
     * Compares blob info.
     */
    friend bool operator!=(const BlobInfo& lhs, const BlobInfo& rhs) { return !(lhs == rhs); };
};

using BlobInfoArray = StaticArray<BlobInfo, cMaxNumBlobs>;

/**
 * Blob URLs request.
 */
struct BlobURLsRequest : public Protocol {
    StaticArray<StaticString<oci::cDigestLen>, cMaxNumBlobs> mDigests;

    /**
     * Compares blob URLs request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator==(const BlobURLsRequest& lhs, const BlobURLsRequest& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mDigests == rhs.mDigests;
    };

    /**
     * Compares blob URLs request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const BlobURLsRequest& lhs, const BlobURLsRequest& rhs) { return !(lhs == rhs); };
};

struct BlobURLsInfo : public Protocol {
    BlobInfoArray mItems;

    /**
     * Compares blob URLs info.
     *
     * @param rhs blob URLs info to compare with.
     * @return bool.
     */
    friend bool operator==(const BlobURLsInfo& lhs, const BlobURLsInfo& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mItems == rhs.mItems;
    };

    /**
     * Compares blob URLs info.
     *
     * @param rhs blob URLs info to compare with.
     * @return bool.
     */
    friend bool operator!=(const BlobURLsInfo& lhs, const BlobURLsInfo& rhs) { return !(lhs == rhs); };
};

} // namespace aos

#endif
