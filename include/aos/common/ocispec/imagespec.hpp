/*
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IMAGESPEC_HPP_
#define AOS_IMAGESPEC_HPP_

#include "aos/common/ocispec/common.hpp"
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
    ContentDescriptor*                            mAosService;

    /**
     * Compares image manifest.
     *
     * @param manifest manifest to compare.
     * @return bool.
     */
    bool operator==(const ImageManifest& manifest) const
    {
        return mSchemaVersion == manifest.mSchemaVersion && mMediaType == manifest.mMediaType
            && mConfig == manifest.mConfig && mLayers == manifest.mLayers
            && ((!mAosService && !manifest.mAosService)
                || (mAosService && manifest.mAosService && *mAosService == *manifest.mAosService));
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
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mEnv;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mEntryPoint;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mCmd;

    /**
     * Compares image config.
     *
     * @param config image config to compare.
     * @return bool.
     */
    bool operator==(const ImageConfig& config) const
    {
        return mEnv == config.mEnv && mEntryPoint == config.mEntryPoint && mCmd == config.mCmd;
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
 * OCI image specification.
 */
struct ImageSpec {
    ImageConfig mConfig;

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
