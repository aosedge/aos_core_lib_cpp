/**
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_RESOURCEMANAGERSTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_RESOURCEMANAGERSTUB_HPP_

#include <map>

#include <core/cm/unitconfig/itf/nodeconfigprovider.hpp>

namespace aos::cm::resourcemanager {

class ResourceManagerStub : public unitconfig::NodeConfigProviderItf {
public:
    void Init() { mConfigs.clear(); }

    void SetNodeConfig(const String& nodeID, const String& nodeType, const NodeConfig& cfg)
    {
        mConfigs[std::make_pair(StaticString<cIDLen>(nodeID), StaticString<cNodeTypeLen>(nodeType))] = cfg;
    }

    Error GetNodeConfig(const String& nodeID, const String& nodeType, NodeConfig& nodeConfig) override
    {
        auto key = std::make_pair(StaticString<cIDLen>(nodeID), StaticString<cNodeTypeLen>(nodeType));

        auto it = mConfigs.find(key);
        if (it == mConfigs.end()) {
            return ErrorEnum::eNotFound;
        }

        nodeConfig = it->second;

        return ErrorEnum::eNone;
    }

private:
    std::map<std::pair<StaticString<cIDLen>, StaticString<cNodeTypeLen>>, NodeConfig> mConfigs;
};

} // namespace aos::cm::resourcemanager

#endif
