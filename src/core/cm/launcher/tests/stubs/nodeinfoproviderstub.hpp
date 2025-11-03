/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_NODEINFOPROVIDERSTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_NODEINFOPROVIDERSTUB_HPP_

#include <map>

#include <core/cm/nodeinfoprovider/itf/nodeinfoprovider.hpp>

namespace aos::cm::nodeinfoprovider {

class NodeInfoProviderStub : public NodeInfoProviderItf {
public:
    void Init()
    {
        mNodes.clear();
        mListeners.clear();
    }

    void AddNodeInfo(const String& nodeID, const UnitNodeInfo& info)
    {
        mNodes[nodeID] = info;

        for (auto* l : mListeners) {
            l->OnNodeInfoChanged(info);
        }
    }

    Error GetAllNodeIds(Array<StaticString<cIDLen>>& ids) const override
    {
        ids.Clear();

        for (const auto& [id, _] : mNodes) {
            if (auto err = ids.PushBack(id); !err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    Error GetNodeInfo(const String& nodeID, UnitNodeInfo& nodeInfo) const override
    {
        auto it = mNodes.find(nodeID);
        if (it == mNodes.end()) {
            return ErrorEnum::eNotFound;
        }

        nodeInfo = it->second;

        return ErrorEnum::eNone;
    }

    Error SubscribeListener(NodeInfoListenerItf& listener) override
    {
        mListeners.push_back(&listener);

        return ErrorEnum::eNone;
    }

    Error UnsubscribeListener(NodeInfoListenerItf& listener) override
    {
        mListeners.erase(std::remove(mListeners.begin(), mListeners.end(), &listener), mListeners.end());

        return ErrorEnum::eNone;
    }

private:
    std::map<StaticString<cIDLen>, UnitNodeInfo> mNodes;
    std::vector<NodeInfoListenerItf*>            mListeners;
};

} // namespace aos::cm::nodeinfoprovider

#endif
