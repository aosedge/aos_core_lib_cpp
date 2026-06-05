/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_NODEMANAGER_TESTS_MOCKS_STORAGEMOCK_HPP_
#define AOS_CORE_IAM_NODEMANAGER_TESTS_MOCKS_STORAGEMOCK_HPP_

#include <gmock/gmock.h>

#include <core/iam/nodemanager/itf/storage.hpp>

namespace aos::iam::nodemanager {

class StorageMock : public StorageItf {
public:
    MOCK_METHOD(Error, SetNodeInfo, (const NodeInfo&), (override));
    MOCK_METHOD(Error, GetNodeInfo, (const String&, NodeInfo&), (const, override));
    MOCK_METHOD(Error, GetAllNodeIDs, (Array<StaticString<cIDLen>>&), (const, override));
    MOCK_METHOD(Error, RemoveNodeInfo, (const String&), (override));
};

} // namespace aos::iam::nodemanager

#endif
