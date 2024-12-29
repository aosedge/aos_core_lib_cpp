/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OCISPEC_HPP_
#define AOS_OCISPEC_HPP_

#include "aos/common/ocispec/imagespec.hpp"
#include "aos/common/ocispec/runtimespec.hpp"
#include "aos/common/ocispec/serviceconfig.hpp"

namespace aos::oci {

/**
 * OCI spec interface.
 */
class OCISpecItf {
public:
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
     * Loads OCI image spec.
     *
     * @param path file path.
     * @param imageSpec image spec.
     * @return Error.
     */
    virtual Error LoadImageSpec(const String& path, ImageSpec& imageSpec) = 0;

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
     * Loads Aos service config.
     *
     * @param path file path.
     * @param serviceConfig service config.
     * @return Error.
     */
    virtual Error LoadServiceConfig(const String& path, ServiceConfig& serviceConfig) = 0;

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
