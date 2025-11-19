/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_OCISPECMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_OCISPECMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/ocispec/itf/ocispec.hpp>

namespace aos::oci {

/**
 * OCI spec interface.
 */
class OCISpecMock : public OCISpecItf {
public:
    MOCK_METHOD(Error, ContentDescriptorFromFile, (const String& path, ContentDescriptor& descriptor), (override));
    MOCK_METHOD(Error, ContentDescriptorFromJSON, (const String& json, ContentDescriptor& descriptor), (override));
    MOCK_METHOD(Error, SaveContentDescriptor, (const String& path, const ContentDescriptor& descriptor), (override));
    MOCK_METHOD(Error, LoadImageManifest, (const String& path, ImageManifest& manifest), (override));
    MOCK_METHOD(Error, SaveImageManifest, (const String& path, const ImageManifest& manifest), (override));
    MOCK_METHOD(Error, ImageSpecFromFile, (const String& path, ImageSpec& manifest), (override));
    MOCK_METHOD(Error, ImageSpecFromJSON, (const String& json, ImageSpec& manifest), (override));
    MOCK_METHOD(Error, SaveImageSpec, (const String& path, const ImageSpec& manifest), (override));
    MOCK_METHOD(Error, LoadRuntimeSpec, (const String& path, RuntimeSpec& manifest), (override));
    MOCK_METHOD(Error, SaveRuntimeSpec, (const String& path, const RuntimeSpec& manifest), (override));
    MOCK_METHOD(Error, ServiceConfigFromFile, (const String& path, ServiceConfig& manifest), (override));
    MOCK_METHOD(Error, ServiceConfigFromJSON, (const String& json, ServiceConfig& manifest), (override));
    MOCK_METHOD(Error, SaveServiceConfig, (const String& path, const ServiceConfig& manifest), (override));
};

} // namespace aos::oci

#endif
