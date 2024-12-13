/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OCISPEC_STUB_HPP_
#define AOS_OCISPEC_STUB_HPP_

#include "aos/common/ocispec/ocispec.hpp"
#include "aos/common/tools/map.hpp"
#include "aos/common/tools/thread.hpp"

namespace aos::oci {

/**
 * OCI spec stub.
 */
template <size_t cImageManifestSize = 8, size_t cImageSpecSize = 8, size_t cRuntimeSpecSize = 8>
class OCISpecStub : public OCISpecItf {
public:
    /**
     * Loads OCI image manifest.
     *
     * @param path file path.
     * @param manifest image manifest.
     * @return Error.
     */
    Error LoadImageManifest(const String& path, oci::ImageManifest& manifest) override
    {
        LockGuard lock(mMutex);

        const auto& [val, err] = mImageManifests.At(path);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        manifest = val;

        return ErrorEnum::eNone;
    }

    /**
     * Saves OCI image manifest.
     *
     * @param path file path.
     * @param manifest image manifest.
     * @return Error.
     */
    Error SaveImageManifest(const String& path, const oci::ImageManifest& manifest) override
    {
        LockGuard lock(mMutex);

        if (auto err = mImageManifests.Set(path, manifest); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    /**
     * Loads OCI image spec.
     *
     * @param path file path.
     * @param imageSpec image spec.
     * @return Error.
     */
    Error LoadImageSpec(const String& path, oci::ImageSpec& imageSpec) override
    {
        LockGuard lock(mMutex);

        const auto& [val, err] = mImageSpecs.At(path);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        imageSpec = val;

        return ErrorEnum::eNone;
    }

    /**
     * Saves OCI image spec.
     *
     * @param path file path.
     * @param imageSpec image spec.
     * @return Error.
     */
    Error SaveImageSpec(const String& path, const oci::ImageSpec& imageSpec) override
    {
        LockGuard lock(mMutex);

        if (auto err = mImageSpecs.Set(path, imageSpec); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    /**
     * Loads OCI runtime spec.
     *
     * @param path file path.
     * @param runtimeSpec runtime spec.
     * @return Error.
     */
    Error LoadRuntimeSpec(const String& path, oci::RuntimeSpec& runtimeSpec) override
    {
        LockGuard lock(mMutex);

        const auto& [val, err] = mRuntimeSpecs.At(path);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        runtimeSpec = val;

        return ErrorEnum::eNone;
    }

    /**
     * Saves OCI runtime spec.
     *
     * @param path file path.
     * @param runtimeSpec runtime spec.
     * @return Error.
     */
    Error SaveRuntimeSpec(const String& path, const oci::RuntimeSpec& runtimeSpec) override
    {
        LockGuard lock(mMutex);

        if (auto err = mRuntimeSpecs.Set(path, runtimeSpec); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

private:
    Mutex                                                                    mMutex;
    StaticMap<StaticString<cFilePathLen>, ImageManifest, cImageManifestSize> mImageManifests;
    StaticMap<StaticString<cFilePathLen>, ImageSpec, cImageSpecSize>         mImageSpecs;
    StaticMap<StaticString<cFilePathLen>, RuntimeSpec, cRuntimeSpecSize>     mRuntimeSpecs;
};

} // namespace aos::oci

#endif
