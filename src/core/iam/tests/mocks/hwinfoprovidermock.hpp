/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_HWINFOPROVIDERMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_HWINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/iam/currentnode/itf/hwinfoprovider.hpp>

namespace aos::iam::currentnode {

/**
 * Hardware information provider mock.
 */
class HWInfoProviderMock : public HWInfoProviderItf {
public:
    MOCK_METHOD(Error, GetHWID, (String&), (override));
};

} // namespace aos::iam::currentnode

#endif
