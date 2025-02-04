/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IMAGESPEC_HPP_
#define AOS_IMAGESPEC_HPP_

#include "aos/common/ocispec/common.hpp"
#include "aos/common/tools/optional.hpp"
#include "aos/common/types.hpp"

namespace aos::oci {

/**
 * Max media type len.
 */
constexpr auto cMaxMediaTypeLen = AOS_CONFIG_OCISPEC_MEDIA_TYPE_LEN;

/**
 * Max digest len.
 *
 */
constexpr auto cMaxDigestLen = AOS_CONFIG_CRYPTO_SHA1_DIGEST_SIZE;

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

    StaticString<cMaxMediaTypeLen> mMediaType;
    StaticString<cMaxDigestLen>    mDigest;
    uint64_t                       mSize;

    /**
     * Compares content descriptor.
     *
     * @param descriptor content descriptor to compare.
     * @return bool.
     */
    bool operator==(const ContentDescriptor& descriptor) const
    {
        return mMediaType == descriptor.mMediaType && mDigest == descriptor.mDigest && mSize == descriptor.mSize;
    }

    /**
     * Compares content descriptor.
     *
     * @param descriptor content descriptor to compare.
     * @return bool.
     */
    bool operator!=(const ContentDescriptor& descriptor) const { return !operator==(descriptor); }
};

/**
 * OCI image manifest.
 */
struct ImageManifest {
    int                                           mSchemaVersion;
    StaticString<cMaxMediaTypeLen>                mMediaType;
    ContentDescriptor                             mConfig;
    StaticArray<ContentDescriptor, cMaxNumLayers> mLayers;
    Optional<ContentDescriptor>                   mAosService;

    /**
     * Compares image manifest.
     *
     * @param manifest manifest to compare.
     * @return bool.
     */
    bool operator==(const ImageManifest& manifest) const
    {
        return mSchemaVersion == manifest.mSchemaVersion && mMediaType == manifest.mMediaType
            && mConfig == manifest.mConfig && mLayers == manifest.mLayers && mAosService == manifest.mAosService;
    }

    /**
     * Compares image manifest.
     *
     * @param manifest manifest to compare.
     * @return bool.
     */
    bool operator!=(const ImageManifest& manifest) const { return !operator==(manifest); }
};

/**
 * OCI image config.
 */
struct ImageConfig {
    StaticArray<StaticString<cEnvVarLen>, cMaxNumEnvVariables> mEnv;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount>    mEntryPoint;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount>    mCmd;
    StaticString<cFilePathLen>                                 mWorkingDir;

    /**
     * Compares image config.
     *
     * @param config image config to compare.
     * @return bool.
     */
    bool operator==(const ImageConfig& config) const
    {
        return mEnv == config.mEnv && mEntryPoint == config.mEntryPoint && mCmd == config.mCmd
            && mWorkingDir == config.mWorkingDir;
    }

    /**
     * Compares image config.
     *
     * @param config image config to compare.
     * @return bool.
     */
    bool operator!=(const ImageConfig& config) const { return !operator==(config); }
};

/**
 * Describes the platform which the image in the manifest runs on.
 */
struct Platform {
    StaticString<cCPUArchFamilyLen> mArchitecture;
    StaticString<cOSTypeLen>        mOS;
    StaticString<cVersionLen>       mOSVersion;
    StaticString<cCPUArchLen>       mVariant;

    /**
     * Compares platform.
     *
     * @param platform platform to compare.
     * @return bool.
     */
    bool operator==(const Platform& platform) const
    {
        return mArchitecture == platform.mArchitecture && mOS == platform.mOS && mOSVersion == platform.mOSVersion
            && mVariant == platform.mVariant;
    }

    /**
     * Compares platform.
     *
     * @param platform platform to compare.
     * @return bool.
     */
    bool operator!=(const Platform& platform) const { return !operator==(platform); }
};

/**
 * OCI image specification.
 */
struct ImageSpec : public Platform {
    Time                     mCreated;
    StaticString<cAuthorLen> mAuthor;
    ImageConfig              mConfig;

    /**
     * Compares image spec.
     *
     * @param spec image spec to compare.
     * @return bool.
     */
    bool operator==(const ImageSpec& spec) const { return mConfig == spec.mConfig; }

    /**
     * Compares image spec.
     *
     * @param spec image spec to compare.
     * @return bool.
     */
    bool operator!=(const ImageSpec& spec) const { return !operator==(spec); }
};

} // namespace aos::oci

#endif
