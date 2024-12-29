/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OCISPEC_MOCK_HPP_
#define AOS_OCISPEC_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/common/ocispec/ocispec.hpp"

namespace aos::oci {

/**
 * OCI spec interface.
 */
class OCISpecMock : public OCISpecItf {
public:
    MOCK_METHOD(Error, LoadImageManifest, (const String& path, ImageManifest& manifest), (override));
    MOCK_METHOD(Error, SaveImageManifest, (const String& path, const ImageManifest& manifest), (override));
    MOCK_METHOD(Error, LoadImageSpec, (const String& path, ImageSpec& manifest), (override));
    MOCK_METHOD(Error, SaveImageSpec, (const String& path, const ImageSpec& manifest), (override));
    MOCK_METHOD(Error, LoadRuntimeSpec, (const String& path, RuntimeSpec& manifest), (override));
    MOCK_METHOD(Error, SaveRuntimeSpec, (const String& path, const RuntimeSpec& manifest), (override));
    MOCK_METHOD(Error, LoadServiceConfig, (const String& path, ServiceConfig& manifest), (override));
    MOCK_METHOD(Error, SaveServiceConfig, (const String& path, const ServiceConfig& manifest), (override));
};

} // namespace aos::oci

#endif
