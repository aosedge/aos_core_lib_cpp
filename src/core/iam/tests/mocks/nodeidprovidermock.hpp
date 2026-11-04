/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_TESTS_MOCKS_NODEIDPROVIDERMOCK_HPP_
#define AOS_CORE_IAM_TESTS_MOCKS_NODEIDPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/iam/currentnode/itf/nodeidprovider.hpp>

namespace aos::iam::currentnode {

/**
 * Node ID provider mock.
 */
class NodeIDProviderMock : public NodeIDProviderItf {
public:
    MOCK_METHOD(Error, GetNodeID, (String&), (override));
};

} // namespace aos::iam::currentnode

#endif
