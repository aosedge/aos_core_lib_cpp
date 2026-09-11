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
constexpr auto cMediaTypeLayerTar                     = "application/vnd.oci.image.layer.v1.tar";
constexpr auto cMediaTypeLayerTarGZip                 = "application/vnd.oci.image.layer.v1.tar+gzip";
constexpr auto cMediaTypeEmptyBlob                    = "application/vnd.oci.empty.v1+json";
constexpr auto cMediaTypeComponentFullTarGZip         = "application/vnd.aos.image.component.full.v1+gzip";
constexpr auto cMediaTypeComponentFullSquashfs        = "application/vnd.aos.image.component.full.v1+squashfs";
constexpr auto cMediaTypeComponentIncrementalSquashfs = "application/vnd.aos.image.component.inc.v1+squashfs";

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
    friend bool operator==(const Platform& lhs, const Platform& rhs)
    {
        return lhs.mArchitecture == rhs.mArchitecture && lhs.mVariant == rhs.mVariant && lhs.mOS == rhs.mOS
            && lhs.mOSVersion == rhs.mOSVersion && lhs.mOSFeatures == rhs.mOSFeatures;
    };

    /**
     * Compares platform.
     *
     * @param rhs platform to compare.
     * @return bool.
     */
    friend bool operator!=(const Platform& lhs, const Platform& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const ContentDescriptor& lhs, const ContentDescriptor& rhs)
    {
        return lhs.mMediaType == rhs.mMediaType && lhs.mDigest == rhs.mDigest && lhs.mSize == rhs.mSize;
    };

    /**
     * Compares content descriptor.
     *
     * @param rhs content descriptor to compare.
     * @return bool.
     */
    friend bool operator!=(const ContentDescriptor& lhs, const ContentDescriptor& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const IndexContentDescriptor& lhs, const IndexContentDescriptor& rhs)
    {
        return (static_cast<const ContentDescriptor&>(lhs) == rhs) && lhs.mPlatform == rhs.mPlatform;
    };

    /**
     * Compares index content descriptor.
     *
     * @param rhs index content descriptor to compare.
     * @return bool.
     */
    friend bool operator!=(const IndexContentDescriptor& lhs, const IndexContentDescriptor& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * OCI image index.
 */
struct ImageIndex {
    int32_t                                               mSchemaVersion {cSchemeVersion};
    StaticString<cMediaTypeLen>                           mMediaType;
    StaticString<cArtifactTypeLen>                        mArtifactType;
    StaticArray<IndexContentDescriptor, cMaxNumManifests> mManifests;

    /**
     * Compares image index.
     *
     * @param rhs index to compare.
     * @return bool.
     */
    friend bool operator==(const ImageIndex& lhs, const ImageIndex& rhs)
    {
        return lhs.mSchemaVersion == rhs.mSchemaVersion && lhs.mMediaType == rhs.mMediaType
            && lhs.mArtifactType == rhs.mArtifactType && lhs.mManifests == rhs.mManifests;
    };

    /**
     * Compares image index.
     *
     * @param rhs index to compare.
     * @return bool.
     */
    friend bool operator!=(const ImageIndex& lhs, const ImageIndex& rhs) { return !(lhs == rhs); };
};

/**
 * OCI image manifest.
 */
struct ImageManifest {
    int32_t                                       mSchemaVersion {cSchemeVersion};
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
    friend bool operator==(const ImageManifest& lhs, const ImageManifest& rhs)
    {
        return lhs.mSchemaVersion == rhs.mSchemaVersion && lhs.mMediaType == rhs.mMediaType
            && lhs.mArtifactType == rhs.mArtifactType && lhs.mConfig == rhs.mConfig && lhs.mLayers == rhs.mLayers
            && lhs.mItemConfig == rhs.mItemConfig;
    };

    /**
     * Compares image manifest.
     *
     * @param rhs manifest to compare.
     * @return bool.
     */
    friend bool operator!=(const ImageManifest& lhs, const ImageManifest& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const Rootfs& lhs, const Rootfs& rhs)
    {
        return lhs.mDiffIDs == rhs.mDiffIDs && lhs.mType == rhs.mType;
    };

    /**
     * Compares rootfs config.
     *
     * @param rhs rootfs config to compare.
     * @return bool.
     */
    friend bool operator!=(const Rootfs& lhs, const Rootfs& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const Config& lhs, const Config& rhs)
    {
        return lhs.mExposedPorts == rhs.mExposedPorts && lhs.mEnv == rhs.mEnv && lhs.mEntryPoint == rhs.mEntryPoint
            && lhs.mCmd == rhs.mCmd && lhs.mWorkingDir == rhs.mWorkingDir;
    };

    /**
     * Compares image config part.
     *
     * @param rhs image config part to compare.
     * @return bool.
     */
    friend bool operator!=(const Config& lhs, const Config& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const ImageConfig& lhs, const ImageConfig& rhs) { return lhs.mConfig == rhs.mConfig; };

    /**
     * Compares image config.
     *
     * @param rhs image config to compare.
     * @return bool.
     */
    friend bool operator!=(const ImageConfig& lhs, const ImageConfig& rhs) { return !(lhs == rhs); };
};

} // namespace aos::oci

#endif
