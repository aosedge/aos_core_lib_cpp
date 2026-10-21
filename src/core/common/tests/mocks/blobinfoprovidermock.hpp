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
 * Download blob info provider mock.
 */
class DownloadBlobInfoProviderMock : public DownloadBlobInfoProviderItf {
public:
    MOCK_METHOD(Error, GetBlobInfo, (const String& digest, BlobInfo& blobInfo), (override));
};

/**
 * Local blob info provider mock.
 */
class LocalBlobInfoProviderMock : public LocalBlobInfoProviderItf {
public:
    MOCK_METHOD(Error, GetBlobPath, (const String& digest, String& path), (const, override));
};

} // namespace aos::common::imagemanager

#endif
