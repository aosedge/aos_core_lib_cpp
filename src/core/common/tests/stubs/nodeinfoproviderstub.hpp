/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_STUBS_NODEINFOPROVIDERSTUB_HPP_
#define AOS_CORE_COMMON_TESTS_STUBS_NODEINFOPROVIDERSTUB_HPP_

#include <mutex>
#include <vector>

#include <core/common/iamclient/itf/nodeinfoprovider.hpp>

namespace aos::iamclient {

/**
 * Node info provider stub.
 */
class NodeInfoProviderStub : public NodeInfoProviderItf {
public:
    /**
     * Sets node info.
     *
     * @param info node info.
     */
    void SetNodeInfo(const NodeInfo& info)
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mNodeInfos.begin(), mNodeInfos.end(),
            [&info](const auto& nodeInfo) { return nodeInfo.mNodeID == info.mNodeID; });

        if (it != mNodeInfos.end()) {
            *it = info;
        } else {
            mNodeInfos.push_back(info);
        }
    }

    /**
     * Notifies listeners about node info changed.
     *
     * @param info node info.
     */
    void NotifyNodeInfoChanged(const NodeInfo& info)
    {
        std::lock_guard lock {mMutex};

        for (auto* listener : mListeners) {
            listener->OnNodeInfoChanged(info);
        }
    }

    /**
     * Returns ids for all the nodes of the unit.
     *
     * @param[out] ids result node identifiers.
     * @return Error.
     */
    Error GetAllNodeIDs(Array<StaticString<cIDLen>>& ids) const override
    {
        std::lock_guard lock {mMutex};

        for (const auto& info : mNodeInfos) {
            if (auto err = ids.EmplaceBack(info.mNodeID); !err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Returns info for specified node.
     *
     * @param nodeID node identifier.
     * @param[out] nodeInfo result node information.
     * @return Error.
     */
    Error GetNodeInfo(const String& nodeID, NodeInfo& nodeInfo) const override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(
            mNodeInfos.begin(), mNodeInfos.end(), [&nodeID](const auto& info) { return info.mNodeID == nodeID; });

        if (it == mNodeInfos.end()) {
            return ErrorEnum::eNotFound;
        }

        nodeInfo = *it;

        return ErrorEnum::eNone;
    }

    /**
     * Subscribes node info notifications.
     *
     * @param listener node info listener.
     * @return Error.
     */
    Error SubscribeListener(NodeInfoListenerItf& listener) override
    {
        std::lock_guard lock {mMutex};

        mListeners.push_back(&listener);

        return ErrorEnum::eNone;
    }

    /**
     * Unsubscribes from node info notifications.
     *
     * @param listener node info listener.
     * @return Error.
     */
    Error UnsubscribeListener(NodeInfoListenerItf& listener) override
    {
        std::lock_guard lock {mMutex};

        mListeners.erase(std::remove(mListeners.begin(), mListeners.end(), &listener), mListeners.end());

        return ErrorEnum::eNone;
    }

private:
    mutable std::mutex                mMutex;
    std::vector<NodeInfo>             mNodeInfos;
    std::vector<NodeInfoListenerItf*> mListeners;
};

} // namespace aos::iamclient

#endif
