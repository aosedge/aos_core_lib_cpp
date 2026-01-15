/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_HASHERMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_HASHERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/crypto/itf/hash.hpp>

namespace aos::sm::networkmanager {

class HashMock : public crypto::HashItf {
public:
    MOCK_METHOD(Error, Update, (const Array<uint8_t>&), (override));
    MOCK_METHOD(Error, Finalize, (Array<uint8_t>&), (override));
};

class HasherMock : public crypto::HasherItf {
public:
    MOCK_METHOD(RetWithError<UniquePtr<crypto::HashItf>>, CreateHash, (crypto::Hash), (override));
};

} // namespace aos::sm::networkmanager

#endif
