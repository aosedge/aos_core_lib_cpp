/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_RANDOMMOCK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_TESTS_MOCKS_RANDOMMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/crypto/itf/rand.hpp>

namespace aos::sm::networkmanager {

class RandomMock : public crypto::RandomItf {
public:
    MOCK_METHOD(RetWithError<uint64_t>, RandInt, (uint64_t), (override));
    MOCK_METHOD(Error, RandBuffer, (Array<uint8_t>&, size_t), (override));
};

} // namespace aos::sm::networkmanager

#endif
