/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_TESTS_STUBS_SENDERSTUB_HPP_
#define AOS_CORE_COMMON_MONITORING_TESTS_STUBS_SENDERSTUB_HPP_

#include <core/common/monitoring/monitoring.hpp>

namespace aos::monitoring {

/**
 * Sender stub.
 */
class SenderStub : public SenderItf {
public:
    Error SendMonitoringData(const NodeMonitoringData& monitoringData) override
    {
        std::lock_guard lock {mMutex};

        mMonitoringData.push(monitoringData);

        mCondVar.notify_one();

        return ErrorEnum::eNone;
    }

    Error WaitMonitoringData(NodeMonitoringData& monitoringData)
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] { return !mMonitoringData.empty(); })) {
            return ErrorEnum::eTimeout;
        }

        monitoringData = mMonitoringData.front();
        mMonitoringData.pop();

        return ErrorEnum::eNone;
    }

private:
    static constexpr auto cWaitTimeout = std::chrono::seconds {5};

    std::mutex                     mMutex;
    std::condition_variable        mCondVar;
    std::queue<NodeMonitoringData> mMonitoringData {};
};

} // namespace aos::monitoring

#endif
