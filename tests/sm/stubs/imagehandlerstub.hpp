/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IMAGEHANDLER_STUB_HPP_
#define AOS_IMAGEHANDLER_STUB_HPP_

#include "aos/common/tools/map.hpp"
#include "aos/common/tools/utils.hpp"
#include "aos/sm/image/imagehandler.hpp"

namespace aos::sm::image {

/**
 * Image handler stub.
 */
class ImageHandlerStub : public ImageHandlerItf {
public:
    /**
     * Initializes image handler.
     *
     * @param allocator space allocator.
     * @return Error.
     */
    Error Init(spaceallocator::SpaceAllocatorItf& allocator)
    {
        mSpaceAllocator = &allocator;

        return ErrorEnum::eNone;
    }

    /**
     * Installs layer from the provided archive.
     *
     * @param archivePath archive path.
     * @param installBasePath installation base path.
     * @param layer layer info.
     * @param space[out] installed layer space.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    RetWithError<StaticString<cFilePathLen>> InstallLayer(const String& archivePath, const String& installBasePath,
        const LayerInfo& layer, UniquePtr<spaceallocator::SpaceItf>& space) override
    {
        (void)installBasePath;
        (void)layer;

        LockGuard lock(mMutex);

        auto it = mUnpackResults.Find(archivePath);

        if (it == mUnpackResults.end()) {
            return {{}, ErrorEnum::eNotFound};
        }

        assert(mSpaceAllocator);

        Error err;

        Tie(space, err) = mSpaceAllocator->AllocateSpace(1);
        if (!err.IsNone()) {
            return {{}, err};
        }

        fs::MakeDirAll(it->mSecond);

        return {it->mSecond, ErrorEnum::eNone};
    }

    /**
     * Installs service from the provided archive.
     *
     * @param archivePath archive path.
     * @param installBasePath installation base path.
     * @param service service info.
     * @param space[out] installed service space.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    RetWithError<StaticString<cFilePathLen>> InstallService(const String& archivePath, const String& installBasePath,
        const ServiceInfo& service, UniquePtr<spaceallocator::SpaceItf>& space) override
    {
        (void)installBasePath;
        (void)service;

        LockGuard lock(mMutex);

        auto it = mUnpackResults.Find(archivePath);

        if (it == mUnpackResults.end()) {
            return {{}, ErrorEnum::eNotFound};
        }

        assert(mSpaceAllocator);

        Error err;

        Tie(space, err) = mSpaceAllocator->AllocateSpace(1);
        if (!err.IsNone()) {
            return {{}, err};
        }

        fs::MakeDirAll(it->mSecond);

        return {it->mSecond, ErrorEnum::eNone};
    }

    /**
     * Validates service.
     *
     * @param path service path.
     * @return Error.
     */
    Error ValidateService(const String& path) const override
    {
        (void)path;

        return ErrorEnum::eNone;
    }

    /**
     * Calculates digest for the given path or file.
     *
     * @param path root folder or file.
     * @return RetWithError<StaticString<cMaxDigestLen>>.
     */
    RetWithError<StaticString<oci::cMaxDigestLen>> CalculateDigest(const String& path) const override
    {
        LockGuard lock(mMutex);

        LOG_DBG() << "Calculate digest: path=" << path;

        auto it = mDigestResults.find(path.CStr());

        if (it == mDigestResults.end()) {
            LOG_ERR() << "Digest not found: path=" << path;

            return {{}, ErrorEnum::eNotFound};
        }

        return {it->second.c_str()};
    }

    /**
     * Sets install result.
     *
     * @param archivePath archive path.
     * @param unpackedPath unpacked path.
     * @return Error.
     */
    Error SetInstallResult(const String& archivePath, const String& unpackedPath)
    {
        LockGuard lock(mMutex);

        return mUnpackResults.Set(archivePath, unpackedPath);
    }

    /**
     * Sets calculate digest result.
     *
     * @param path path.
     * @param digest digest.
     * @return Error.
     */
    Error SetCalculateDigestResult(const String& path, const String& digest)
    {
        LockGuard lock(mMutex);

        mDigestResults[path.CStr()] = digest.CStr();

        return ErrorEnum::eNone;
    }

private:
    mutable Mutex                      mMutex;
    spaceallocator::SpaceAllocatorItf* mSpaceAllocator = nullptr;
    StaticMap<StaticString<cFilePathLen>, StaticString<cFilePathLen>, cMaxNumLayers + cMaxNumServices> mUnpackResults;
    std::map<std::string, std::string>                                                                 mDigestResults;
};

} // namespace aos::sm::image

#endif
