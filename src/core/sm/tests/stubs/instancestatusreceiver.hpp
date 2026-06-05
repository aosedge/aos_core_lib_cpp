/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_STUBS_INSTANCESTATUSRECEIVER_HPP_
#define AOS_CORE_SM_TESTS_STUBS_INSTANCESTATUSRECEIVER_HPP_

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

#include <core/sm/launcher/launcher.hpp>

namespace aos::sm::launcher {

/**
 * Instance status receiver stub.
 */
class InstanceStatusReceiverStub : public InstanceStatusReceiverItf {
public:
    /**
     * Receives instances statuses.
     *
     * @param statuses instances statuses.
     * @return Error.
     */
    Error OnInstancesStatusesReceived(const Array<InstanceStatus>& statuses) override
    {
        std::lock_guard lock {mMutex};

        for (const auto& status : statuses) {
            mReceivedStatuses.push_back(status);
        }

        mStatusesCondVar.notify_all();

        return ErrorEnum::eNone;
    }

    /**
     * Notifies that runtime requires reboot.
     *
     * @param runtimeID runtime identifier.
     * @return Error.
     */
    Error RebootRequired(const String& runtimeID) override
    {
        std::lock_guard lock {mMutex};

        mRuntimesToReboot.push_back(runtimeID);

        mRuntimesCondVar.notify_all();

        return ErrorEnum::eNone;
    }

    /**
     * Gets received instances statuses.
     *
     * @param statuses output parameter for received statuses.
     * @param timeout maximum time to wait for statuses.
     * @return Error.
     */
    Error GetStatuses(std::vector<InstanceStatus>& statuses, std::chrono::milliseconds timeout)
    {
        std::unique_lock lock {mMutex};

        if (!mStatusesCondVar.wait_for(lock, timeout, [this]() { return !mReceivedStatuses.empty(); })) {
            return ErrorEnum::eTimeout;
        }

        statuses = mReceivedStatuses;
        mReceivedStatuses.clear();

        return ErrorEnum::eNone;
    }

    /**
     * Gets runtimes that require reboot.
     *
     * @param runtimes output parameter for runtime identifiers.
     * @param timeout maximum time to wait for runtimes.
     * @return Error.
     */
    Error GetRuntimesToReboot(std::vector<StaticString<cIDLen>>& runtimes, std::chrono::milliseconds timeout)
    {
        std::unique_lock lock {mMutex};

        if (!mRuntimesCondVar.wait_for(lock, timeout, [this]() { return !mRuntimesToReboot.empty(); })) {
            return ErrorEnum::eTimeout;
        }

        runtimes = mRuntimesToReboot;
        mRuntimesToReboot.clear();

        return ErrorEnum::eNone;
    }

private:
    std::mutex              mMutex;
    std::condition_variable mStatusesCondVar;
    std::condition_variable mRuntimesCondVar;

    std::vector<InstanceStatus>       mReceivedStatuses;
    std::vector<StaticString<cIDLen>> mRuntimesToReboot;
};

} // namespace aos::sm::launcher

#endif
