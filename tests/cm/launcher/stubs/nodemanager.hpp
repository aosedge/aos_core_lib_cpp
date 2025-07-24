/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CM_LAUNCHER_STUBS_NODEMANAGER_HPP_
#define AOS_CM_LAUNCHER_STUBS_NODEMANAGER_HPP_

#include <algorithm>
#include <aos/cm/launcher/nodemanager.hpp>
#include <map>
#include <string>
#include <vector>

namespace aos::cm::nodemanager {

/**
 * Start request for testing.
 */
struct StartRequest {
    std::vector<ServiceInfo>  mServices;
    std::vector<LayerInfo>    mLayers;
    std::vector<InstanceInfo> mInstances;
    bool                      mForceRestart;
};

/**
 * Copies unique items only
 */
template <typename SourceContainer, typename DestContainer, typename Cmp>
void CopyUnique(const SourceContainer& source, DestContainer& destination, Cmp cmp)
{
    for (const auto& srcItem : source) {
        bool skip = false;
        for (const auto& dstItem : destination) {
            if (cmp(srcItem, dstItem)) {
                skip = true;
                break;
            }
        }

        if (!skip) {
            destination.push_back(srcItem);
        }
    }
}

/**
 * Stub implementation for NodeManagerItf interface.
 */
class NodeManagerStub : public NodeManagerItf {
public:
    /**
     * Initializes stub object.
     */
    void Init()
    {
        mRunRequests.clear();
        mMonitoring.clear();
        mListeners.clear();
    }

    /**
     * Runs service instances on the specified node.
     *
     * @param nodeID node identifier.
     * @param services service list.
     * @param layers service layer list.
     * @param instances service instance list.
     * @param forceRestart force service restart.
     * @return Error.
     */
    Error StartInstances(const String& nodeID, const Array<ServiceInfo>& services, const Array<LayerInfo>& layers,
        const Array<InstanceInfo>& instances, bool forceRestart) override
    {
        auto& request = mRunRequests[nodeID];

        auto newServices  = ConvertToVector(services);
        auto newLayers    = ConvertToVector(layers);
        auto newInstances = ConvertToVector(instances);

        CopyUnique(newServices, request.mServices,
            [](const ServiceInfo& left, const ServiceInfo& right) { return left.mServiceID == right.mServiceID; });

        CopyUnique(newLayers, request.mLayers,
            [](const LayerInfo& left, const LayerInfo& right) { return left.mLayerDigest == right.mLayerDigest; });

        CopyUnique(newInstances, request.mInstances, [](const InstanceInfo& left, const InstanceInfo& right) {
            return left.mInstanceIdent == right.mInstanceIdent;
        });

        request.mForceRestart = forceRestart;

        // Create success status
        NodeRunInstanceStatus successStatus;

        successStatus.mNodeID   = nodeID;
        successStatus.mNodeType = "test-node-type";

        for (const auto& instance : request.mInstances) {
            InstanceStatus instanceStatus;

            instanceStatus.mInstanceIdent  = instance.mInstanceIdent;
            instanceStatus.mServiceVersion = "1.0";
            instanceStatus.mRunState       = InstanceRunStateEnum::eActive;
            instanceStatus.mNodeID         = nodeID;

            if (auto err = successStatus.mInstances.PushBack(instanceStatus); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        SendRunStatus(successStatus);

        return Error();
    }

    /**
     * Stops service instances on the specified node.
     *
     * @param nodeID node identifier.
     * @param instances service instance list.
     * @return Error.
     */
    Error StopInstances(const String& nodeID, const Array<InstanceIdent>& instances) override
    {
        NodeRunInstanceStatus stopStatus;
        stopStatus.mNodeID   = nodeID;
        stopStatus.mNodeType = "test-node-type";

        auto& nodeInstances = mRunRequests[nodeID].mInstances;

        for (auto it = nodeInstances.begin(); it != nodeInstances.end();) {
            if (instances.Exist(it->mInstanceIdent)) {
                it = nodeInstances.erase(it);
            } else {
                it++;
            }
        }

        for (size_t i = 0; i < nodeInstances.size(); ++i) {
            InstanceStatus instanceStatus;

            instanceStatus.mInstanceIdent  = nodeInstances[i].mInstanceIdent;
            instanceStatus.mServiceVersion = "1.0";
            instanceStatus.mRunState       = InstanceRunStateEnum::eActive;
            instanceStatus.mNodeID         = nodeID;

            if (auto err = stopStatus.mInstances.PushBack(instanceStatus); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        SendRunStatus(stopStatus);

        return Error();
    }

    /**
     * Retrieves average monitoring data for a node.
     *
     * @param nodeID node identifier.
     * @param monitoring average monitoring data.
     * @return Error.
     */
    Error GetAverageMonitoring(const String& nodeID, monitoring::NodeMonitoringData& monitoring) const override
    {
        auto it = mMonitoring.find(nodeID);
        if (it != mMonitoring.end()) {
            monitoring = it->second;
        } else {
            // Return empty monitoring data if not found
            monitoring = monitoring::NodeMonitoringData {};
        }

        return Error();
    }

    /**
     * Subscribes listener of service statuses.
     *
     * @param listener service status listener.
     * @return Error.
     */
    Error SubscribeListener(ServiceStatusListenerItf& listener) override
    {
        mListeners.push_back(&listener);
        return Error();
    }

    /**
     * Unsubscribes listener of service statuses.
     *
     * @param listener service status listener.
     * @return Error.
     */
    Error UnsubscribeListener(ServiceStatusListenerItf& listener) override
    {
        auto it = std::find(mListeners.begin(), mListeners.end(), &listener);
        if (it != mListeners.end()) {
            mListeners.erase(it);
        }
        return Error();
    }

    /**
     * Helper method to add monitoring data for testing.
     *
     * @param nodeID node identifier.
     * @param monitoring monitoring data.
     */
    void AddMonitoring(const String& nodeID, const monitoring::NodeMonitoringData& monitoring)
    {
        mMonitoring[nodeID] = monitoring;
    }

    /**
     * Helper method to clear all monitoring data.
     */
    void ClearMonitoring() { mMonitoring.clear(); }

    /**
     * Helper method to get run requests for testing.
     *
     * @param nodeID node identifier.
     * @return StartRequest pointer or nullptr if not found.
     */
    const StartRequest* GetRunRequest(const String& nodeID) const
    {
        auto it = mRunRequests.find(nodeID);
        return (it != mRunRequests.end()) ? &it->second : nullptr;
    }

    /**
     * Helper method to clear all run requests.
     */
    void ClearRunRequests() { mRunRequests.clear(); }

    /**
     * Helper method to compare run requests for testing.
     *
     * @param expectedRunRequests expected run requests map.
     * @return Error if comparison fails.
     */
    Error CompareStartRequests(const std::map<std::string, StartRequest>& expectedRunRequests) const
    {
        if (expectedRunRequests.size() != mRunRequests.size()) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        for (const auto& pair : mRunRequests) {
            const String&       nodeID        = pair.first;
            const StartRequest& actualRequest = pair.second;

            auto expectedIt = expectedRunRequests.find(nodeID.CStr());
            if (expectedIt == expectedRunRequests.end()) {
                return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
            }

            const StartRequest& expectedRequest = expectedIt->second;

            if (!CompareArrays(actualRequest.mServices, expectedRequest.mServices)) {
                return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
            }

            if (!CompareArrays(actualRequest.mLayers, expectedRequest.mLayers)) {
                return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
            }

            if (!CompareArrays(actualRequest.mInstances, expectedRequest.mInstances)) {
                return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
            }

            if (actualRequest.mForceRestart != expectedRequest.mForceRestart) {
                return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
            }
        }

        return Error();
    }

    void SendRunStatus(const NodeRunInstanceStatus& status)
    {
        for (auto* listener : mListeners) {
            listener->OnStatusChanged(status);
        }
    }

    std::map<String, StartRequest> GetStartRequests() { return mRunRequests; }

private:
    template <typename T>
    std::vector<T> ConvertToVector(const Array<T>& array)
    {
        return std::vector<T> {array.begin(), array.end()};
    }

    template <typename T>
    bool CompareArrays(const std::vector<T>& actual, const std::vector<T>& expected) const
    {
        if (actual.size() != expected.size()) {
            return false;
        }

        for (size_t i = 0; i < actual.size(); ++i) {
            if (!(actual[i] == expected[i])) {
                return false;
            }
        }

        return true;
    }

    std::map<String, StartRequest>                   mRunRequests;
    std::map<String, monitoring::NodeMonitoringData> mMonitoring;
    std::vector<ServiceStatusListenerItf*>           mListeners;
};

} // namespace aos::cm::nodemanager

#endif
