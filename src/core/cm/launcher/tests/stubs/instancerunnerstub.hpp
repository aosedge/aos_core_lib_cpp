/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_INSTANCERUNNERSTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_INSTANCERUNNERSTUB_HPP_

#include <gmock/gmock.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <core/cm/launcher/itf/instancerunner.hpp>
#include <core/cm/launcher/itf/instancestatusreceiver.hpp>
#include <core/common/tools/logger.hpp>
#include <core/common/types/instance.hpp>

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

    void Init(InstanceStatusReceiverItf& statusReceiver, bool autoUpdateStatuses = true,
        aos::InstanceStateEnum initialState = aos::InstanceStateEnum::eActivating)
    {
        mNodeInstances.clear();
        mInstanceStatuses.clear();
        mPreinstalledComponents.clear();
        mAutoUpdateStatuses = autoUpdateStatuses;
        mStatusReceiver     = &statusReceiver;
        mInitialState       = initialState;
    }

    const std::map<std::string, NodeRunRequest>& GetRunRequests() const { return mNodeInstances; }

    void SetAutoUpdateStatuses(bool enable)
    {
        std::lock_guard lock {mMutex};

        mAutoUpdateStatuses = enable;
    }

    void SetInstanceStatuses(const std::vector<InstanceStatus>& statuses)
    {
        std::lock_guard lock {mMutex};

        mInstanceStatuses = statuses;
    }

    void SetPreinstalledComponents(const std::vector<InstanceStatus>& preinstalledComponents)
    {
        std::lock_guard lock {mMutex};

        mPreinstalledComponents = preinstalledComponents;
    }

    MOCK_METHOD(void, OnRunRequest, ());

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

        if (mStatusReceiver != nullptr) {
            std::shared_ptr<std::vector<InstanceStatus>> statuses;

            {
                std::lock_guard lock {mMutex};

                if (mAutoUpdateStatuses) {
                    mInstanceStatuses.clear();
                    mInstanceStatuses.reserve(startInstances.Size() + mPreinstalledComponents.size());

                    for (const auto& inst : startInstances) {
                        InstanceStatus status;

                        static_cast<InstanceIdent&>(status) = static_cast<const InstanceIdent&>(inst);
                        status.mNodeID                      = nodeID;
                        status.mRuntimeID                   = inst.mRuntimeID;
                        status.mState                       = mInitialState;
                        status.mError                       = ErrorEnum::eNone;

                        mInstanceStatuses.push_back(status);
                    }

                    for (const auto& preinstalled : mPreinstalledComponents) {
                        mInstanceStatuses.push_back(preinstalled);
                    }
                }

                statuses = std::make_shared<std::vector<InstanceStatus>>(mInstanceStatuses);
            }

            OnRunRequest();

            std::thread([receiver = mStatusReceiver, nodeID, statuses]() mutable {
                Array<InstanceStatus> arr(statuses->data(), statuses->size());
                receiver->OnNodeInstancesStatusesReceived(nodeID, arr);
            }).detach();
        }

        return ErrorEnum::eNone;
    }

private:
    mutable std::mutex mMutex;

    std::map<std::string, NodeRunRequest> mNodeInstances {};
    InstanceStatusReceiverItf*            mStatusReceiver {};

    bool                        mAutoUpdateStatuses {true};
    std::vector<InstanceStatus> mInstanceStatuses {};
    std::vector<InstanceStatus> mPreinstalledComponents {};
    aos::InstanceStateEnum      mInitialState {aos::InstanceStateEnum::eActivating};
};

} // namespace aos::cm::launcher

#endif
