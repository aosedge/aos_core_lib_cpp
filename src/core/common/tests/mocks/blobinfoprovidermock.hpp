/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_BLOBINFOPROVIDERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_BLOBINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/imagemanager/itf/blobinfoprovider.hpp>

namespace aos::common::imagemanager {

/**
 * Blob info provider mock.
 */
class BlobInfoProviderMock : public BlobInfoProviderItf {
public:
    MOCK_METHOD(Error, GetBlobInfo, (const String& digest, BlobInfo& blobInfo), (override));
};

} // namespace aos::common::imagemanager

#endif
