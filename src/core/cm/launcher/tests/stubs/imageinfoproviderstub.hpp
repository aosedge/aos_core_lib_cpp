/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_STUBS_IMAGEINFOPROVIDERSTUB_HPP_
#define AOS_CORE_CM_LAUNCHER_STUBS_IMAGEINFOPROVIDERSTUB_HPP_

#include <map>
#include <vector>

#include <core/cm/launcher/itf/imageinfoprovider.hpp>
#include <core/common/ocispec/ocispec.hpp>

namespace aos::cm::launcher {

class ImageInfoProviderStub : public ImageInfoProviderItf {
public:
    void Init()
    {
        mItemVersions.clear();
        mItemImages.clear();
        mServiceConfigs.clear();
        mImageConfigs.clear();
        mServiceGIDs.clear();
        mNextGID = 1000;
    }

    void SetServiceConfig(const String& id, const String& imageID, const oci::ServiceConfig& cfg)
    {
        mServiceConfigs[{StaticString<cIDLen>(id), StaticString<cIDLen>(imageID)}] = cfg;
        EnsureImageInfo(id, imageID);
    }

    void SetImageConfig(const String& id, const String& imageID, const oci::ImageConfig& cfg)
    {
        mImageConfigs[{StaticString<cIDLen>(id), StaticString<cIDLen>(imageID)}] = cfg;
        EnsureImageInfo(id, imageID);
    }

    void SetServiceGID(const String& id, gid_t gid) { mServiceGIDs[id] = gid; }

    RetWithError<StaticString<cVersionLen>> GetItemVersion(const String& id) override
    {
        auto it = mItemVersions.find(id);
        if (it == mItemVersions.end()) {
            return {StaticString<cVersionLen> {}, ErrorEnum::eNotFound};
        }

        return {it->second, ErrorEnum::eNone};
    }

    Error GetItemImages(const String& id, Array<ImageInfo>& imagesInfos) override
    {
        auto it = mItemImages.find(id);
        if (it == mItemImages.end()) {
            return ErrorEnum::eNotFound;
        }

        for (const auto& info : it->second) {
            [[maybe_unused]] auto err = imagesInfos.PushBack(info);
        }

        return ErrorEnum::eNone;
    }

    Error GetServiceConfig(const String& id, const String& imageID, oci::ServiceConfig& config) override
    {
        auto key = Key {id, imageID};

        auto it = mServiceConfigs.find(key);
        if (it == mServiceConfigs.end()) {
            return ErrorEnum::eNotFound;
        }

        config = it->second;

        return ErrorEnum::eNone;
    }

    Error GetImageConfig(const String& id, const String& imageID, oci::ImageConfig& config) override
    {
        auto key = Key {StaticString<cIDLen>(id), StaticString<cIDLen>(imageID)};

        auto it = mImageConfigs.find(key);
        if (it == mImageConfigs.end()) {
            return ErrorEnum::eNotFound;
        }

        config = it->second;

        return ErrorEnum::eNone;
    }

    RetWithError<gid_t> GetServiceGID(const String& id) override
    {
        auto it = mServiceGIDs.find(id);
        if (it == mServiceGIDs.end()) {
            gid_t newGid     = static_cast<gid_t>(mNextGID++);
            mServiceGIDs[id] = newGid;

            return {newGid, ErrorEnum::eNone};
        }

        return {it->second, ErrorEnum::eNone};
    }

private:
    struct Key {
        StaticString<cIDLen> mItemID;
        StaticString<cIDLen> mImageID;

        bool operator<(const Key& other) const
        {
            if (mItemID != other.mItemID) {
                return mItemID < other.mItemID;
            }

            return mImageID < other.mImageID;
        }
    };

    void EnsureImageInfo(const String& id, const String& imageID)
    {
        auto& images = mItemImages[id];
        bool  exists = false;

        for (const auto& info : images) {
            if (info.mImageID == imageID) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            ImageInfo info;
            info.mImageID = imageID;
            images.push_back(info);
        }
    }

    std::map<StaticString<cIDLen>, StaticString<cVersionLen>> mItemVersions;
    std::map<StaticString<cIDLen>, std::vector<ImageInfo>>    mItemImages;
    std::map<Key, oci::ServiceConfig>                         mServiceConfigs;
    std::map<Key, oci::ImageConfig>                           mImageConfigs;
    std::map<StaticString<cIDLen>, gid_t>                     mServiceGIDs;
    size_t                                                    mNextGID {1000};
};

} // namespace aos::cm::launcher

#endif
