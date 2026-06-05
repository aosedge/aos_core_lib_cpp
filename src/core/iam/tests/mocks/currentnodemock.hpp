/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_CURRENTNODEMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_CURRENTNODEMOCK_HPP_

#include <gmock/gmock.h>

#include <core/iam/currentnode/itf/currentnodehandler.hpp>

namespace aos::iam::currentnode {

/**
 * Current node handler mock.
 */
class CurrentNodeHandlerMock : public CurrentNodeHandlerItf {
public:
    MOCK_METHOD(Error, SetState, (NodeState), (override));
    MOCK_METHOD(Error, SetConnected, (bool), (override));
    MOCK_METHOD(Error, GetCurrentNodeInfo, (NodeInfo&), (const, override));
    MOCK_METHOD(Error, SubscribeListener, (iamclient::CurrentNodeInfoListenerItf&), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (iamclient::CurrentNodeInfoListenerItf&), (override));
};

} // namespace aos::iam::currentnode

#endif
