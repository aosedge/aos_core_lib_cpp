/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_OCISPEC_IMAGESPEC_HPP_
#define AOS_CORE_COMMON_OCISPEC_IMAGESPEC_HPP_

#include <core/common/tools/optional.hpp>
#include <core/common/types/envvars.hpp>
#include <core/common/types/network.hpp>

#include "common.hpp"

namespace aos::oci {

constexpr auto cSchemeVersion = 2;

/**
 * Scheme version.
 */
constexpr auto cSchemaVersion = 2;

/**
 * Max media type len.
 */
constexpr auto cMediaTypeLen = AOS_CONFIG_OCISPEC_MEDIA_TYPE_LEN;

/**
 * Max artifact type len.
 */
constexpr auto cArtifactTypeLen = AOS_CONFIG_OCISPEC_ARTIFACT_TYPE_LEN;

/**
 * Max digest len.
 */
constexpr auto cDigestLen = AOS_CONFIG_OCISPEC_DIGEST_LEN;

/**
 * Max num manifests.
 */
constexpr auto cMaxNumManifests = AOS_CONFIG_OCISPEC_MAX_NUM_MANIFESTS;

/**
 * Max num layers.
 */
constexpr auto cMaxNumLayers = AOS_CONFIG_OCISPEC_MAX_NUM_LAYERS;

/**
 * Rootfs type len.
 */
constexpr auto cRootfsTypeLen = AOS_CONFIG_OCISPEC_ROOTFS_TYPE_LEN;

/**
 * OCI layer media type constants.
 */
constexpr auto cMediaTypeLayerTar     = "application/vnd.oci.image.layer.v1.tar";
constexpr auto cMediaTypeLayerTarGZip = "application/vnd.oci.image.layer.v1.tar+gzip";
constexpr auto cMediaTypeEmptyBlob    = "application/vnd.oci.empty.v1+json";

/**
 * Describes the platform which the image in the manifest runs on.
 */
struct Platform {
    StaticString<cCPUArchLen>                                  mArchitecture;
    StaticString<cCPUVariantLen>                               mVariant;
    StaticString<cOSTypeLen>                                   mOS;
    StaticString<cVersionLen>                                  mOSVersion;
    StaticArray<StaticString<cOSFeatureLen>, cOSFeaturesCount> mOSFeatures;

    /**
     * Compares platform.
     *
     * @param rhs platform to compare.
     * @return bool.
     */
    bool operator==(const Platform& rhs) const
    {
        return mArchitecture == rhs.mArchitecture && mVariant == rhs.mVariant && mOS == rhs.mOS
            && mOSVersion == rhs.mOSVersion && mOSFeatures == rhs.mOSFeatures;
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
 * OCI content descriptor.
 */
struct ContentDescriptor {
    StaticString<cMediaTypeLen> mMediaType;
    StaticString<cDigestLen>    mDigest;
    uint64_t                    mSize {};

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
 * OCI index file content descriptor.
 */
struct IndexContentDescriptor : public ContentDescriptor {
    Optional<Platform> mPlatform;

    /**
     * Compares index content descriptor.
     *
     * @param rhs index content descriptor to compare.
     * @return bool.
     */
    bool operator==(const IndexContentDescriptor& rhs) const
    {
        return ContentDescriptor::operator==(rhs) && mPlatform == rhs.mPlatform;
    }

    /**
     * Compares index content descriptor.
     *
     * @param rhs index content descriptor to compare.
     * @return bool.
     */
    bool operator!=(const IndexContentDescriptor& rhs) const { return !operator==(rhs); }
};

/**
 * OCI image index.
 */
struct ImageIndex {
    int                                                   mSchemaVersion {cSchemeVersion};
    StaticString<cMediaTypeLen>                           mMediaType;
    StaticString<cArtifactTypeLen>                        mArtifactType;
    StaticArray<IndexContentDescriptor, cMaxNumManifests> mManifests;

    /**
     * Compares image index.
     *
     * @param rhs index to compare.
     * @return bool.
     */
    bool operator==(const ImageIndex& rhs) const
    {
        return mSchemaVersion == rhs.mSchemaVersion && mMediaType == rhs.mMediaType
            && mArtifactType == rhs.mArtifactType && mManifests == rhs.mManifests;
    }

    /**
     * Compares image index.
     *
     * @param rhs index to compare.
     * @return bool.
     */
    bool operator!=(const ImageIndex& rhs) const { return !operator==(rhs); }
};

/**
 * OCI image manifest.
 */
struct ImageManifest {
    int                                           mSchemaVersion {cSchemeVersion};
    StaticString<cMediaTypeLen>                   mMediaType;
    StaticString<cArtifactTypeLen>                mArtifactType;
    ContentDescriptor                             mConfig;
    StaticArray<ContentDescriptor, cMaxNumLayers> mLayers;
    Optional<ContentDescriptor>                   mItemConfig;

    /**
     * Compares image manifest.
     *
     * @param rhs manifest to compare.
     * @return bool.
     */
    bool operator==(const ImageManifest& rhs) const
    {
        return mSchemaVersion == rhs.mSchemaVersion && mMediaType == rhs.mMediaType
            && mArtifactType == rhs.mArtifactType && mConfig == rhs.mConfig && mLayers == rhs.mLayers
            && mItemConfig == rhs.mItemConfig;
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
 * Rootfs config struct.
 */
struct Rootfs {
    StaticArray<StaticString<cDigestLen>, cMaxNumLayers> mDiffIDs;
    StaticString<cRootfsTypeLen>                         mType;

    /**
     * Compares rootfs config.
     *
     * @param rhs rootfs config to compare.
     * @return bool.
     */
    bool operator==(const Rootfs& rhs) const { return mDiffIDs == rhs.mDiffIDs && mType == rhs.mType; }

    /**
     * Compares rootfs config.
     *
     * @param rhs rootfs config to compare.
     * @return bool.
     */
    bool operator!=(const Rootfs& rhs) const { return !operator==(rhs); }
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
        return mExposedPorts == rhs.mExposedPorts && mEnv == rhs.mEnv && mEntryPoint == rhs.mEntryPoint
            && mCmd == rhs.mCmd && mWorkingDir == rhs.mWorkingDir;
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
 * OCI image config.
 */
struct ImageConfig : public Platform {
    Time                     mCreated;
    StaticString<cAuthorLen> mAuthor;
    Config                   mConfig;
    Rootfs                   mRootfs;

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
