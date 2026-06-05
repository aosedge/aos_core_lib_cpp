/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_TESTS_MOCKS_IMAGEHANDLERMOCK_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_TESTS_MOCKS_IMAGEHANDLERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/imagemanager/itf/imagehandler.hpp>

namespace aos::sm::imagemanager {

/**
 * Image handler mock.
 */
class ImageHandlerMock : public ImageHandlerItf {
public:
    MOCK_METHOD(Error, UnpackLayer, (const String&, const String&, const String&), (override));
    MOCK_METHOD(RetWithError<size_t>, GetUnpackedLayerSize, (const String&, const String&), (const, override));
    MOCK_METHOD(
        RetWithError<StaticString<oci::cDigestLen>>, GetUnpackedLayerDigest, (const String&), (const, override));
};

} // namespace aos::sm::imagemanager

#endif
