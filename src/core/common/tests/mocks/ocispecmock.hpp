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
    MOCK_METHOD(Error, LoadImageIndex, (const String&, ImageIndex&), (override));
    MOCK_METHOD(Error, SaveImageIndex, (const String&, const ImageIndex&), (override));
    MOCK_METHOD(Error, LoadImageManifest, (const String&, ImageManifest&), (override));
    MOCK_METHOD(Error, SaveImageManifest, (const String&, const ImageManifest&), (override));
    MOCK_METHOD(Error, LoadImageConfig, (const String&, ImageConfig&), (override));
    MOCK_METHOD(Error, SaveImageConfig, (const String&, const ImageConfig&), (override));
    MOCK_METHOD(Error, LoadItemConfig, (const String&, ItemConfig&), (override));
    MOCK_METHOD(Error, SaveItemConfig, (const String&, const ItemConfig&), (override));
    MOCK_METHOD(Error, LoadRuntimeConfig, (const String&, RuntimeConfig&), (override));
    MOCK_METHOD(Error, SaveRuntimeConfig, (const String&, const RuntimeConfig&), (override));
};

} // namespace aos::oci

#endif
