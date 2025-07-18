/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CM_LAUNCHER_STUBS_RESOURCEMANAGER_HPP_
#define AOS_CM_LAUNCHER_STUBS_RESOURCEMANAGER_HPP_

#include <aos/cm/resourcemanager.hpp>
#include <map>
#include <string>

namespace aos::cm::resourcemanager {

/**
 * Stub implementation for ResourceManagerItf interface.
 */
class ResourceManagerStub : public ResourceManagerItf {
public:
    /**
     * Initializes stub object.
     */
    void Init(const std::map<std::string, NodeConfig>& configs = {}) { mNodeConfigs = configs; }

    /**
     * Returns node config.
     *
     * @param nodeID node identifier.
     * @param nodeType node type.
     * @param[out] nodeConfig param to store node config.
     * @return Error.
     */
    Error GetNodeConfig(const String& nodeID, const String& nodeType, NodeConfig& nodeConfig) override
    {
        (void)nodeID;

        mNodeConfigs[nodeType.CStr()].mNodeType = nodeType;
        nodeConfig                              = mNodeConfigs[nodeType.CStr()];

        return Error();
    }

    /**
     * Helper method to add node config for testing.
     *
     * @param nodeID node identifier.
     * @param nodeConfig node configuration.
     */
    void AddNodeConfig(const std::string& nodeType, const NodeConfig& config) { mNodeConfigs[nodeType] = config; }

    /**
     * Helper method to clear all node configs.
     */
    void ClearNodeConfigs() { mNodeConfigs.clear(); }

private:
    std::map<std::string, NodeConfig> mNodeConfigs;
};

} // namespace aos::cm::resourcemanager

#endif
