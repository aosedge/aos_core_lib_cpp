/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_OCISPEC_OCISPEC_HPP_
#define AOS_CORE_COMMON_OCISPEC_OCISPEC_HPP_

#include "imagespec.hpp"
#include "runtimespec.hpp"
#include "serviceconfig.hpp"

namespace aos::oci {

/**
 * OCI spec interface.
 */
class OCISpecItf {
public:
    /**
     * Loads OCI content descriptor from file.
     *
     * @param path file path.
     * @param descriptor[out]  content descriptor.
     * @return Error.
     */
    virtual Error ContentDescriptorFromFile(const String& path, ContentDescriptor& descriptor) = 0;

    /**
     * Loads OCI content descriptor from json string.
     *
     * @param json json string.
     * @param descriptor[out]  content descriptor.
     * @return Error.
     */
    virtual Error ContentDescriptorFromJSON(const String& json, ContentDescriptor& descriptor) = 0;

    /**
     * Saves OCI content descriptor.
     *
     * @param path file path.
     * @param descriptor[out] content descriptor.
     * @return Error.
     */
    virtual Error SaveContentDescriptor(const String& path, const ContentDescriptor& descriptor) = 0;

    /**
     * Loads OCI image manifest.
     *
     * @param path file path.
     * @param manifest image manifest.
     * @return Error.
     */
    virtual Error LoadImageManifest(const String& path, ImageManifest& manifest) = 0;

    /**
     * Saves OCI image manifest.
     *
     * @param path file path.
     * @param manifest image manifest.
     * @return Error.
     */
    virtual Error SaveImageManifest(const String& path, const ImageManifest& manifest) = 0;

    /**
     * Loads OCI image spec from file.
     *
     * @param path file path.
     * @param imageSpec image spec.
     * @return Error.
     */
    virtual Error ImageSpecFromFile(const String& path, ImageSpec& imageSpec) = 0;

    /**
     * Loads OCI image spec from json string.
     *
     * @param json json string.
     * @param imageSpec image spec.
     * @return Error.
     */
    virtual Error ImageSpecFromJSON(const String& json, ImageSpec& imageSpec) = 0;

    /**
     * Saves OCI image spec.
     *
     * @param path file path.
     * @param imageSpec image spec.
     * @return Error.
     */
    virtual Error SaveImageSpec(const String& path, const ImageSpec& imageSpec) = 0;

    /**
     * Loads OCI runtime spec.
     *
     * @param path file path.
     * @param runtimeSpec runtime spec.
     * @return Error.
     */
    virtual Error LoadRuntimeSpec(const String& path, RuntimeSpec& runtimeSpec) = 0;

    /**
     * Saves OCI runtime spec.
     *
     * @param path file path.
     * @param runtimeSpec runtime spec.
     * @return Error.
     */
    virtual Error SaveRuntimeSpec(const String& path, const RuntimeSpec& runtimeSpec) = 0;

    /**
     * Loads Aos service config from file.
     *
     * @param path file path.
     * @param serviceConfig service config.
     * @return Error.
     */
    virtual Error ServiceConfigFromFile(const String& path, ServiceConfig& serviceConfig) = 0;

    /**
     * Loads Aos service config from json string.
     *
     * @param json json string.
     * @param serviceConfig service config.
     * @return Error.
     */
    virtual Error ServiceConfigFromJSON(const String& json, ServiceConfig& serviceConfig) = 0;

    /**
     * Saves Aos service config.
     *
     * @param path file path.
     * @param serviceConfig service config.
     * @return Error.
     */
    virtual Error SaveServiceConfig(const String& path, const ServiceConfig& serviceConfig) = 0;

    /**
     * Destroys OCI spec interface.
     */
    virtual ~OCISpecItf() = default;
};

} // namespace aos::oci

#endif
