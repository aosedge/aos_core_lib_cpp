/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_NODEMANAGER_TESTS_STORAGEMOCK_HPP_
#define AOS_CORE_IAM_NODEMANAGER_TESTS_STORAGEMOCK_HPP_

#include <gmock/gmock.h>

#include <core/iam/nodemanager/itf/storage.hpp>

namespace aos::iam::nodemanager {

class NodeInfoStorageMock : public NodeInfoStorageItf {
public:
    MOCK_METHOD(Error, SetNodeInfo, (const NodeInfo& info), (override));
    MOCK_METHOD(Error, GetNodeInfo, (const String& nodeID, NodeInfo& nodeInfo), (const, override));
    MOCK_METHOD(Error, GetAllNodeIDs, (Array<StaticString<cIDLen>> & ids), (const, override));
    MOCK_METHOD(Error, RemoveNodeInfo, (const String& nodeID), (override));
};

} // namespace aos::iam::nodemanager

#endif
