/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_OCISPEC_IMAGESPEC_HPP_
#define AOS_CORE_COMMON_OCISPEC_IMAGESPEC_HPP_

#include <core/common/tools/optional.hpp>
#include <core/common/types/obsolete.hpp>

#include "common.hpp"

namespace aos::oci {

/**
 * Scheme version.
 */
constexpr auto cSchemaVersion = 2;

/**
 * Max media type len.
 */
constexpr auto cMediaTypeLen = AOS_CONFIG_OCISPEC_MEDIA_TYPE_LEN;

/**
 * Max digest len.
 *
 */
constexpr auto cDigestLen = AOS_CONFIG_CRYPTO_SHA1_DIGEST_SIZE;

/**
 * OCI content descriptor.
 */

struct ContentDescriptor {
    /**
     * Crates content descriptor.
     */
    ContentDescriptor() = default;

    /**
     * Creates content descriptor.
     *
     * @param mediaType media type.
     * @param digest digest.
     * @param size size.
     */
    ContentDescriptor(const String& mediaType, const String& digest, uint64_t size)
        : mMediaType(mediaType)
        , mDigest(digest)
        , mSize(size)
    {
    }

    StaticString<cMediaTypeLen> mMediaType;
    StaticString<cDigestLen>    mDigest;
    uint64_t                    mSize = 0;

    /**
     * Compares content descriptor.
     *
     * @param rhs content descriptor to compare.
     * @return bool.
     */
    bool operator==(const ContentDescriptor& rhs) const
    {
        return mMediaType == rhs.mMediaType && mDigest == rhs.mDigest && mSize == rhs.mSize;
    }

    /**
     * Compares content descriptor.
     *
     * @param rhs content descriptor to compare.
     * @return bool.
     */
    bool operator!=(const ContentDescriptor& rhs) const { return !operator==(rhs); }
};

/**
 * OCI image manifest.
 */
struct ImageManifest {
    int                                           mSchemaVersion {cSchemaVersion};
    StaticString<cMediaTypeLen>                   mMediaType;
    ContentDescriptor                             mConfig;
    StaticArray<ContentDescriptor, cMaxNumLayers> mLayers;
    Optional<ContentDescriptor>                   mAosService;

    /**
     * Compares image manifest.
     *
     * @param rhs manifest to compare.
     * @return bool.
     */
    bool operator==(const ImageManifest& rhs) const
    {
        return mSchemaVersion == rhs.mSchemaVersion && mMediaType == rhs.mMediaType && mConfig == rhs.mConfig
            && mLayers == rhs.mLayers && mAosService == rhs.mAosService;
    }

    /**
     * Compares image manifest.
     *
     * @param rhs manifest to compare.
     * @return bool.
     */
    bool operator!=(const ImageManifest& rhs) const { return !operator==(rhs); }
};

/**
 * OCI image config part.
 */
struct Config {
    StaticArray<StaticString<cExposedPortLen>, cMaxNumExposedPorts> mExposedPorts;
    StaticArray<StaticString<cEnvVarLen>, cMaxNumEnvVariables>      mEnv;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount>         mEntryPoint;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount>         mCmd;
    StaticString<cFilePathLen>                                      mWorkingDir;

    /**
     * Compares image config part.
     *
     * @param rhs image config part to compare.
     * @return bool.
     */
    bool operator==(const Config& rhs) const
    {
        return mEnv == rhs.mEnv && mEntryPoint == rhs.mEntryPoint && mCmd == rhs.mCmd && mWorkingDir == rhs.mWorkingDir;
    }

    /**
     * Compares image config part.
     *
     * @param rhs image config part to compare.
     * @return bool.
     */
    bool operator!=(const Config& rhs) const { return !operator==(rhs); }
};

/**
 * Describes the platform which the image in the manifest runs on.
 */
struct Platform {
    StaticString<cCPUArchLen>    mArchitecture;
    StaticString<cOSTypeLen>     mOS;
    StaticString<cVersionLen>    mOSVersion;
    StaticString<cCPUVariantLen> mVariant;

    /**
     * Compares platform.
     *
     * @param rhs platform to compare.
     * @return bool.
     */
    bool operator==(const Platform& rhs) const
    {
        return mArchitecture == rhs.mArchitecture && mOS == rhs.mOS && mOSVersion == rhs.mOSVersion
            && mVariant == rhs.mVariant;
    }

    /**
     * Compares platform.
     *
     * @param rhs platform to compare.
     * @return bool.
     */
    bool operator!=(const Platform& rhs) const { return !operator==(rhs); }
};

/**
 * OCI image config.
 */
struct ImageConfig : public Platform {
    Time                     mCreated;
    StaticString<cAuthorLen> mAuthor;
    Config                   mConfig;

    /**
     * Compares image config.
     *
     * @param rhs image config to compare.
     * @return bool.
     */
    bool operator==(const ImageConfig& rhs) const { return mConfig == rhs.mConfig; }

    /**
     * Compares image config.
     *
     * @param rhs image config to compare.
     * @return bool.
     */
    bool operator!=(const ImageConfig& rhs) const { return !operator==(rhs); }
};

} // namespace aos::oci

#endif
