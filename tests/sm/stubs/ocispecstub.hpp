/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OCISPEC_STUB_HPP_
#define AOS_OCISPEC_STUB_HPP_

#include <mutex>
#include <string>
#include <unordered_map>

#include "aos/common/ocispec/ocispec.hpp"

namespace aos::oci {

/**
 * OCI spec stub.
 */
class OCISpecStub : public OCISpecItf {
public:
    /**
     * Loads OCI content descriptor.
     *
     * @param path file path.
     * @param descriptor[out]  content descriptor.
     * @return Error.
     */
    Error LoadContentDescriptor(const String& path, ContentDescriptor& descriptor) override
    {
        std::lock_guard lock {mMutex};

        if (mContentDescriptors.find(path.CStr()) == mContentDescriptors.end()) {
            return ErrorEnum::eNotFound;
        }

        descriptor = mContentDescriptors.at(path.CStr());

        return ErrorEnum::eNone;
    }

    /**
     * Saves OCI content descriptor.
     *
     * @param path file path.
     * @param descriptor[out] content descriptor.
     * @return Error.
     */
    Error SaveContentDescriptor(const String& path, const ContentDescriptor& descriptor) override
    {
        std::lock_guard lock {mMutex};

        mContentDescriptors[path.CStr()] = descriptor;

        return ErrorEnum::eNone;
    }

    /**
     * Loads OCI image manifest.
     *
     * @param path file path.
     * @param manifest image manifest.
     * @return Error.
     */
    Error LoadImageManifest(const String& path, oci::ImageManifest& manifest) override
    {
        std::lock_guard lock {mMutex};

        if (mImageManifests.find(path.CStr()) == mImageManifests.end()) {
            return ErrorEnum::eNotFound;
        }

        manifest = mImageManifests.at(path.CStr());

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
        std::lock_guard lock {mMutex};

        mImageManifests[path.CStr()] = manifest;

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
        std::lock_guard lock {mMutex};

        if (mImageSpecs.find(path.CStr()) == mImageSpecs.end()) {
            return ErrorEnum::eNotFound;
        }

        imageSpec = mImageSpecs.at(path.CStr());

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
        std::lock_guard lock {mMutex};

        mImageSpecs[path.CStr()] = imageSpec;

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
        std::lock_guard lock {mMutex};

        if (mImageSpecs.find(path.CStr()) == mImageSpecs.end()) {
            return ErrorEnum::eNotFound;
        }

        runtimeSpec = mRuntimeSpecs.at(path.CStr());

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
        std::lock_guard lock {mMutex};

        mRuntimeSpecs[path.CStr()] = runtimeSpec;

        return ErrorEnum::eNone;
    }

    /**
     * Loads Aos service config.
     *
     * @param path file path.
     * @param serviceConfig service config.
     * @return Error.
     */
    Error LoadServiceConfig(const String& path, ServiceConfig& serviceConfig) override
    {
        std::lock_guard lock {mMutex};

        if (mServiceConfigs.find(path.CStr()) == mServiceConfigs.end()) {
            return ErrorEnum::eNotFound;
        }

        serviceConfig = mServiceConfigs.at(path.CStr());

        return ErrorEnum::eNone;
    }

    /**
     * Saves Aos service config.
     *
     * @param path file path.
     * @param serviceConfig service config.
     * @return Error.
     */
    Error SaveServiceConfig(const String& path, const ServiceConfig& serviceConfig) override
    {
        std::lock_guard lock {mMutex};

        mServiceConfigs[path.CStr()] = serviceConfig;

        return ErrorEnum::eNone;
    }

private:
    std::mutex                               mMutex;
    std::map<std::string, ContentDescriptor> mContentDescriptors;
    std::map<std::string, ImageManifest>     mImageManifests;
    std::map<std::string, ImageSpec>         mImageSpecs;
    std::map<std::string, RuntimeSpec>       mRuntimeSpecs;
    std::map<std::string, ServiceConfig>     mServiceConfigs;
};

} // namespace aos::oci

#endif
