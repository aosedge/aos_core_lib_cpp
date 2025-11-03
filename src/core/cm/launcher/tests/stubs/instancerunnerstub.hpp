/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_INSTANCERUNNERSTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_INSTANCERUNNERSTUB_HPP_

#include <map>
#include <string>

#include <core/cm/launcher/itf/instancerunner.hpp>

namespace aos::cm::launcher {

class InstanceRunnerStub : public InstanceRunnerItf {
public:
    struct NodeRunRequest {
        std::vector<aos::InstanceInfo> mStopInstances;
        std::vector<aos::InstanceInfo> mStartInstances;

        bool operator==(const NodeRunRequest& other) const
        {
            return mStopInstances == other.mStopInstances && mStartInstances == other.mStartInstances;
        }

        bool operator!=(const NodeRunRequest& other) const { return !(*this == other); }
    };

    void Init() { mNodeInstances.clear(); }

    const std::map<std::string, NodeRunRequest>& GetNodeInstances() const { return mNodeInstances; }

    const NodeRunRequest* GetNodeInstances(const String& nodeID) const
    {
        auto it = mNodeInstances.find(nodeID.CStr());
        if (it != mNodeInstances.end()) {
            return &it->second;
        }

        return nullptr;
    }

    // InstanceRunnerItf
    Error UpdateInstances(const String& nodeID, const Array<aos::InstanceInfo>& stopInstances,
        const Array<aos::InstanceInfo>& startInstances) override
    {
        // Update the map with nodeID -> node run request
        NodeRunRequest& nodeRequest = mNodeInstances[nodeID.CStr()];
        nodeRequest.mStopInstances.clear();
        nodeRequest.mStartInstances.clear();

        for (const auto& inst : stopInstances) {
            nodeRequest.mStopInstances.push_back(inst);
        }

        for (const auto& inst : startInstances) {
            nodeRequest.mStartInstances.push_back(inst);
        }

        return ErrorEnum::eNone;
    }

private:
    std::map<std::string, NodeRunRequest> mNodeInstances {};
};

} // namespace aos::cm::launcher

#endif
