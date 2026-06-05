/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_BLOBINFOPROVIDER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_BLOBINFOPROVIDER_HPP_

#include <gmock/gmock.h>

#include <core/cm/imagemanager/itf/blobinfoprovider.hpp>

namespace aos::cm::imagemanager {

/**
 * Blob info provider mock.
 */
class BlobInfoProviderMock : public BlobInfoProviderItf {
public:
    MOCK_METHOD(Error, GetBlobsInfos, (const Array<StaticString<oci::cDigestLen>>&, Array<BlobInfo>&), (override));
};

} // namespace aos::cm::imagemanager

#endif
