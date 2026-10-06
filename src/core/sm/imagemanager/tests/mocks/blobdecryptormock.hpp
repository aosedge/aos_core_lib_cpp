/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_IMAGEMANAGER_TESTS_MOCKS_BLOBDECRYPTORMOCK_HPP_
#define AOS_CORE_SM_IMAGEMANAGER_TESTS_MOCKS_BLOBDECRYPTORMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/imagemanager/itf/blobdecryptor.hpp>

namespace aos::sm::imagemanager {

/**
 * Blob decryptor mock.
 */
class BlobDecryptorMock : public BlobDecryptorItf {
public:
    MOCK_METHOD(Error, Decrypt, (const String& encryptedPath, const String& decryptedPath), (override));
};

} // namespace aos::sm::imagemanager

#endif
