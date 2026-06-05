/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_NODEHANDLERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_NODEHANDLERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/iamclient/itf/nodehandler.hpp>

namespace aos::iamclient {

/**
 * Node handler mock.
 */
class NodeHandlerMock : public NodeHandlerItf {
public:
    MOCK_METHOD(Error, PauseNode, (const String&), (override));
    MOCK_METHOD(Error, ResumeNode, (const String&), (override));
};

} // namespace aos::iamclient

#endif
