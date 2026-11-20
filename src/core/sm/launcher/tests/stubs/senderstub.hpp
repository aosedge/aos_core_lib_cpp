/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_TESTS_STUBS_SENDERSTUB_HPP_
#define AOS_CORE_SM_LAUNCHER_TESTS_STUBS_SENDERSTUB_HPP_

#include <condition_variable>
#include <mutex>
#include <queue>

#include <core/sm/launcher/itf/sender.hpp>

namespace aos::sm::launcher {

/**
 * Sender stub.
 */
class SenderStub : public SenderItf {
public:
    void SetSendError(const Error& err)
    {
        std::lock_guard lock {mMutex};

        mSendError = err;
    }

    Error SendNodeInstancesStatuses(const Array<aos::InstanceStatus>& statuses) override
    {
        std::lock_guard lock {mMutex};

        mStatusesQueue.push(statuses);
        mCondVar.notify_one();

        return mSendError;
    }

    Error SendUpdateInstancesStatuses(const Array<aos::InstanceStatus>& statuses) override
    {
        std::lock_guard lock {mMutex};

        mStatusesQueue.push(statuses);
        mCondVar.notify_one();

        return mSendError;
    }

    Error WaitStatuses(Array<InstanceStatus>& statuses, Duration timeout)
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, std::chrono::milliseconds(timeout.Milliseconds()),
                [this]() { return !mStatusesQueue.empty(); })) {
            return ErrorEnum::eTimeout;
        }

        statuses = mStatusesQueue.front();
        mStatusesQueue.pop();

        return ErrorEnum::eNone;
    }

private:
    std::mutex                      mMutex;
    std::condition_variable         mCondVar;
    std::queue<InstanceStatusArray> mStatusesQueue;
    Error                           mSendError;
};

} // namespace aos::sm::launcher

#endif
