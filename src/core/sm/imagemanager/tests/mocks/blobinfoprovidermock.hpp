/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_TESTS_MOCKS_BLOBINFOPROVIDERMOCK_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_TESTS_MOCKS_BLOBINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/imagemanager/itf/blobinfoprovider.hpp>

namespace aos::sm::imagemanager {

/**
 * Blob info provider mock.
 */
class BlobInfoProviderMock : public BlobInfoProviderItf {
public:
    MOCK_METHOD(
        Error, GetBlobsInfo, (const Array<StaticString<oci::cDigestLen>>&, Array<StaticString<cURLLen>>&), (override));
};

} // namespace aos::sm::imagemanager

#endif
