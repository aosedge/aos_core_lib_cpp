/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_TESTS_STUBS_NODEINFOPROVIDERSTUB_HPP_
#define AOS_CORE_COMMON_MONITORING_TESTS_STUBS_NODEINFOPROVIDERSTUB_HPP_

#include <core/common/iamclient/itf/nodeinfoprovider.hpp>

namespace aos::monitoring {

/**
 * Node info provider stub.
 */
class NodeInfoProviderStub : public iam::nodeinfoprovider::NodeInfoProviderItf {
public:
    NodeInfoProviderStub(const NodeInfoObsolete& nodeInfo)
        : mNodeInfo(nodeInfo)
    {
    }

    Error GetNodeInfo(NodeInfoObsolete& nodeInfo) const override
    {
        nodeInfo = mNodeInfo;

        return ErrorEnum::eNone;
    }

    Error SetNodeState(const NodeStateObsolete& state) override
    {
        (void)state;

        return ErrorEnum::eNone;
    }

    Error SubscribeNodeStateChanged(iam::nodeinfoprovider::NodeStateObserverItf& observer)
    {
        (void)observer;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeNodeStateChanged(iam::nodeinfoprovider::NodeStateObserverItf& observer)
    {
        (void)observer;

        return ErrorEnum::eNone;
    }

private:
    NodeInfoObsolete mNodeInfo {};
};

} // namespace aos::monitoring

#endif
