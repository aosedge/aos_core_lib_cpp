/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_NODEINFOPROVIDER_HPP_
#define AOS_CM_LAUNCHER_STUBS_NODEINFOPROVIDER_HPP_

#include <aos/cm/nodeinfoprovider.hpp>
#include <map>

namespace aos::cm::nodeinfoprovider {

/**
 * Stub implementation for NodeInfoProviderItf interface.
 */
class NodeInfoProviderStub : public NodeInfoProviderItf {

public:
    /**
     * Initializes stub object.
     *
     * @param nodeID current node identifier.
     */
    void Init(const String& nodeID) { mNodeID = nodeID; }

    /**
     * Retrieves current node identifier.
     *
     * @return StaticString<cNodeIDLen>.
     */
    StaticString<cNodeIDLen> GetCurrentNodeID() const override { return mNodeID; }

    /**
     * Returns info for specified node.
     *
     * @param nodeID node identifier.
     * @param[out] nodeInfo result node information.
     * @return Error.
     */
    Error GetNodeInfo(const String& nodeID, NodeInfo& nodeInfo) const override
    {
        auto it = mNodeInfo.find(nodeID);
        if (it == mNodeInfo.end()) {
            return Error(ErrorEnum::eNotFound);
        }

        nodeInfo = it->second;

        return Error();
    }

    /**
     * Returns ids for all the nodes of the unit.
     *
     * @param[out] ids result node identifiers.
     * @return Error.
     */
    Error GetAllNodeIds(Array<StaticString<cNodeIDLen>>& ids) const override
    {
        ids.Clear();

        for (const auto& pair : mNodeInfo) {
            if (auto err = ids.PushBack(StaticString<cNodeIDLen>(pair.first)); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        return Error();
    }

    /**
     * Helper method to add node info for testing.
     *
     * @param nodeID node identifier.
     * @param info node information.
     */
    void AddNodeInfo(const String& nodeID, const NodeInfo& info) { mNodeInfo[nodeID] = info; }

    /**
     * Helper method to clear all node info.
     */
    void ClearNodeInfo() { mNodeInfo.clear(); }

private:
    StaticString<cNodeIDLen>   mNodeID;
    std::map<String, NodeInfo> mNodeInfo;
};

} // namespace aos::cm::nodeinfoprovider

#endif
