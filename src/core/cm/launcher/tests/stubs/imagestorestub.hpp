/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_TESTS_STUBS_IMAGESTORESTUB_HPP_
#define AOS_CORE_CM_LAUNCHER_TESTS_STUBS_IMAGESTORESTUB_HPP_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <core/cm/imagemanager/itf/blobinfoprovider.hpp>
#include <core/cm/imagemanager/itf/iteminfoprovider.hpp>
#include <core/common/ocispec/itf/ocispec.hpp>

namespace aos::cm::imagemanager {

/**
 * Test helper that emulates OCI images.
 * Provides ItemInfoProvider, BlobInfoProvider, and OCISpec interfaces.
 */
class ImageStoreStub : public ItemInfoProviderItf, public oci::OCISpecItf {
public:
    void Init()
    {
        mItemVersions.clear();
        mIndexDigests.clear();
        mImageIndexes.clear();
        mImageManifests.clear();
        mImageConfigs.clear();
        mItemConfigs.clear();
        mKnownDigests.clear();
    }

    void AddItem(const String& itemID, const String& imageID, const oci::ItemConfig& itemCfg,
        const oci::ImageConfig& imageCfg, const String& version = DefaultVersion)
    {
        auto versionStr              = version.IsEmpty() ? DefaultVersion : version.CStr();
        mItemVersions[itemID.CStr()] = versionStr;

        auto manifestDigest = EnsureImageArtifacts(itemID, versionStr, imageID);
        auto configDigest   = MakeDigest(itemID, imageID, "config");
        auto itemDigest     = MakeDigest(itemID, imageID, "item");

        mImageConfigs[configDigest] = imageCfg;
        mItemConfigs[itemDigest]    = itemCfg;

        auto& manifest = mImageManifests[manifestDigest];
        manifest.mItemConfig.EmplaceValue();
        manifest.mItemConfig->mDigest    = itemDigest.c_str();
        manifest.mItemConfig->mMediaType = cItemMediaType;

        mKnownDigests.insert(configDigest);
        mKnownDigests.insert(itemDigest);
    }

    StaticString<oci::cDigestLen> GetManifestDigest(const String& itemID, const String& imageID) const
    {
        StaticString<oci::cDigestLen> digest;
        digest = MakeDigest(itemID, imageID, "manifest").c_str();

        return digest;
    }

    static StaticString<oci::cDigestLen> BuildManifestDigest(const String& itemID, const String& imageID)
    {
        StaticString<oci::cDigestLen> digest;
        digest = MakeDigest(itemID, imageID, "manifest").c_str();

        return digest;
    }

    //
    // imagemanager::ItemInfoProviderItf
    //
    Error GetIndexDigest(const String& itemID, const String& version, String& digest) const override
    {
        auto it = mIndexDigests.find(ItemKey {itemID.CStr(), version.CStr()});
        if (it == mIndexDigests.end()) {
            return ErrorEnum::eNotFound;
        }

        return digest.Assign(it->second.c_str());
    }

    Error GetBlobPath(const String& digest, String& path) const override
    {
        if (!mKnownDigests.count(digest.CStr())) {
            return ErrorEnum::eNotFound;
        }

        return path.Assign(digest);
    }

    Error GetBlobURL(const String&, String&) const override { return ErrorEnum::eNotSupported; }

    Error GetItemCurrentVersion(const String& itemID, String& version) const override
    {
        auto it = mItemVersions.find(itemID.CStr());
        if (it == mItemVersions.end()) {
            return ErrorEnum::eNotFound;
        }

        return version.Assign(it->second.c_str());
    }

    //
    // oci::OCISpecItf
    //
    Error LoadImageIndex(const String& path, oci::ImageIndex& index) override
    {
        auto it = mImageIndexes.find(path.CStr());
        if (it == mImageIndexes.end()) {
            return ErrorEnum::eNotFound;
        }

        index = it->second;
        return ErrorEnum::eNone;
    }

    Error SaveImageIndex(const String&, const oci::ImageIndex&) override { return ErrorEnum::eNotSupported; }

    Error LoadImageManifest(const String& path, oci::ImageManifest& manifest) override
    {
        auto it = mImageManifests.find(path.CStr());
        if (it == mImageManifests.end()) {
            return ErrorEnum::eNotFound;
        }

        manifest = it->second;
        return ErrorEnum::eNone;
    }

    Error SaveImageManifest(const String&, const oci::ImageManifest&) override { return ErrorEnum::eNotSupported; }

    Error LoadImageConfig(const String& path, oci::ImageConfig& imageConfig) override
    {
        auto it = mImageConfigs.find(path.CStr());
        if (it == mImageConfigs.end()) {
            return ErrorEnum::eNotFound;
        }

        imageConfig = it->second;
        return ErrorEnum::eNone;
    }

    Error SaveImageConfig(const String&, const oci::ImageConfig&) override { return ErrorEnum::eNotSupported; }

    Error LoadItemConfig(const String& path, oci::ItemConfig& itemConfig) override
    {
        auto it = mItemConfigs.find(path.CStr());
        if (it == mItemConfigs.end()) {
            return ErrorEnum::eNotFound;
        }

        itemConfig = it->second;
        return ErrorEnum::eNone;
    }

    Error SaveItemConfig(const String&, const oci::ItemConfig&) override { return ErrorEnum::eNotSupported; }
    Error LoadRuntimeConfig(const String&, oci::RuntimeConfig&) override { return ErrorEnum::eNotSupported; }
    Error SaveRuntimeConfig(const String&, const oci::RuntimeConfig&) override { return ErrorEnum::eNotSupported; }

private:
    struct ItemKey {
        std::string mItemID;
        std::string mVersion;

        bool operator<(const ItemKey& rhs) const
        {
            return std::tie(mItemID, mVersion) < std::tie(rhs.mItemID, rhs.mVersion);
        }
    };

    static constexpr const char* DefaultVersion     = "";
    static constexpr const char* cImageManifestType = "application/vnd.oci.image.manifest.v1+json";
    static constexpr const char* cImageConfigType   = "application/vnd.oci.image.config.v1+json";
    static constexpr const char* cItemMediaType     = "application/vnd.aos.item.config.v1+json";

    static std::string MakeDigest(const String& itemID, const String& suffix, const char* descriptor)
    {
        return std::string(itemID.CStr()) + ":" + suffix.CStr() + ":" + descriptor;
    }

    static std::string MakeDigest(const String& itemID, const std::string& suffix, const char* descriptor)
    {
        return std::string(itemID.CStr()) + ":" + suffix + ":" + descriptor;
    }

    std::string EnsureImageArtifacts(const String& itemID, const char* version, const String& imageID)
    {
        auto itemKey        = ItemKey {itemID.CStr(), version};
        auto indexDigest    = MakeDigest(itemID, std::string(version), "index");
        auto manifestDigest = MakeDigest(itemID, imageID, "manifest");
        auto configDigest   = MakeDigest(itemID, imageID, "config");

        mIndexDigests[itemKey] = indexDigest;

        auto& index = mImageIndexes[indexDigest];
        index.mManifests.Clear();
        oci::IndexContentDescriptor descriptor;
        descriptor.mDigest    = manifestDigest.c_str();
        descriptor.mMediaType = cImageManifestType;
        index.mManifests.PushBack(descriptor);

        auto& manifest              = mImageManifests[manifestDigest];
        manifest.mMediaType         = cImageManifestType;
        manifest.mConfig.mDigest    = configDigest.c_str();
        manifest.mConfig.mMediaType = cImageConfigType;
        manifest.mLayers.Clear();

        mKnownDigests.insert(indexDigest);
        mKnownDigests.insert(manifestDigest);
        mKnownDigests.insert(configDigest);

        return manifestDigest;
    }

    std::map<std::string, std::string>        mItemVersions;
    std::map<ItemKey, std::string>            mIndexDigests;
    std::map<std::string, oci::ImageIndex>    mImageIndexes;
    std::map<std::string, oci::ImageManifest> mImageManifests;
    std::map<std::string, oci::ImageConfig>   mImageConfigs;
    std::map<std::string, oci::ItemConfig>    mItemConfigs;
    std::set<std::string>                     mKnownDigests;
};

} // namespace aos::cm::imagemanager

#endif
