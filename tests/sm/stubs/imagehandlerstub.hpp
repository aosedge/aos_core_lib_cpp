/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IMAGEHANDLER_STUB_HPP_
#define AOS_IMAGEHANDLER_STUB_HPP_

#include "aos/common/image/imagehandler.hpp"
#include "aos/common/tools/map.hpp"
#include "aos/common/tools/utils.hpp"

namespace aos::imagehandler {

/**
 * Image handler stub.
 */
class ImageHandlerStub : public ImageHandlerItf {
public:
    Error Init(spaceallocator::SpaceAllocatorItf& allocator)
    {
        mSpaceAllocator = &allocator;

        return ErrorEnum::eNone;
    }

    RetWithError<StaticString<cFilePathLen>> InstallLayer(
        const String& archivePath, UniquePtr<spaceallocator::SpaceItf>& space) const override
    {
        LockGuard lock(mMutex);

        auto [val, err] = mUnpackResults.At(archivePath);

        if (!err.IsNone()) {
            return {{}, ErrorEnum::eNotFound};
        }

        assert(mSpaceAllocator);

        Tie(space, err) = mSpaceAllocator->AllocateSpace(1);
        if (!err.IsNone()) {
            return {{}, err};
        }

        FS::MakeDirAll(val);

        return {val, ErrorEnum::eNone};
    }

    RetWithError<StaticString<cFilePathLen>> InstallService(
        const String& archivePath, UniquePtr<spaceallocator::SpaceItf>& space) const override
    {
        LockGuard lock(mMutex);

        auto [val, err] = mUnpackResults.At(archivePath);

        if (!err.IsNone()) {
            return {{}, ErrorEnum::eNotFound};
        }

        assert(mSpaceAllocator);

        Tie(space, err) = mSpaceAllocator->AllocateSpace(1);
        if (!err.IsNone()) {
            return {{}, err};
        }

        FS::MakeDirAll(val);

        return {val, ErrorEnum::eNone};
    }

    Error ValidateService(const String& path) const override
    {
        (void)path;

        return ErrorEnum::eNone;
    }

    RetWithError<StaticString<oci::cMaxDigestLen>> CalculateDigest(const String& path) const override
    {
        (void)path;

        return {{}, ErrorEnum::eFailed};
    }

    Error SetInstallResult(const String& archivePath, const String& unpackedPath)
    {
        LockGuard lock(mMutex);

        return mUnpackResults.Set(archivePath, unpackedPath);
    }

private:
    mutable Mutex                      mMutex;
    spaceallocator::SpaceAllocatorItf* mSpaceAllocator = nullptr;
    StaticMap<StaticString<cFilePathLen>, StaticString<cFilePathLen>, cMaxNumLayers + cMaxNumServices> mUnpackResults;
};

} // namespace aos::imagehandler

#endif
