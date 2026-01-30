/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_OCISPEC_OCISPEC_HPP_
#define AOS_CORE_COMMON_OCISPEC_OCISPEC_HPP_

#include "imagespec.hpp"
#include "itemconfig.hpp"
#include "runtimespec.hpp"

namespace aos::oci {

/**
 * OCI spec interface.
 */
class OCISpecItf {
public:
    /**
     * Loads OCI image index.
     *
     * @param path file path.
     * @param index image index.
     * @return Error.
     */
    virtual Error LoadImageIndex(const String& path, ImageIndex& index) = 0;

    /**
     * Saves OCI image index.
     *
     * @param path file path.
     * @param index image index.
     * @return Error.
     */
    virtual Error SaveImageIndex(const String& path, const ImageIndex& index) = 0;

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
     * Loads OCI image config.
     *
     * @param path file path.
     * @param imageConfig image config.
     * @return Error.
     */
    virtual Error LoadImageConfig(const String& path, ImageConfig& imageConfig) = 0;

    /**
     * Saves OCI image config.
     *
     * @param path file path.
     * @param imageConfig image config.
     * @return Error.
     */
    virtual Error SaveImageConfig(const String& path, const ImageConfig& imageConfig) = 0;

    /**
     * Loads Aos item config.
     *
     * @param path file path.
     * @param itemConfig item config.
     * @return Error.
     */
    virtual Error LoadItemConfig(const String& path, ItemConfig& itemConfig) = 0;

    /**
     * Saves Aos item config.
     *
     * @param path file path.
     * @param itemConfig item config.
     * @return Error.
     */
    virtual Error SaveItemConfig(const String& path, const ItemConfig& itemConfig) = 0;

    /**
     * Loads OCI runtime config.
     *
     * @param path file path.
     * @param runtimeConfig runtime config.
     * @return Error.
     */
    virtual Error LoadRuntimeConfig(const String& path, RuntimeConfig& runtimeConfig) = 0;

    /**
     * Saves OCI runtime config.
     *
     * @param path file path.
     * @param runtimeConfig runtime config.
     * @return Error.
     */
    virtual Error SaveRuntimeConfig(const String& path, const RuntimeConfig& runtimeConfig) = 0;

    /**
     * Destroys OCI spec interface.
     */
    virtual ~OCISpecItf() = default;
};

} // namespace aos::oci

#endif
