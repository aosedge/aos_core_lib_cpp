/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_SENDERSTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_SENDERSTUB_HPP_

#include <chrono>
#include <condition_variable>
#include <mutex>

#include <core/cm/launcher/itf/sender.hpp>

namespace aos::cm::launcher {

class SenderStub : public SenderItf {
public:
    Error SendOverrideEnvsStatuses(const OverrideEnvVarsStatuses& statuses) override
    {
        std::lock_guard lock {mMutex};

        mStatuses = statuses;
        ++mSendCount;

        mCondVar.notify_all();

        return ErrorEnum::eNone;
    }

    OverrideEnvVarsStatuses GetOverrideEnvVarsStatuses() const
    {
        std::lock_guard lock {mMutex};

        return mStatuses;
    }

    bool WaitForSendCount(size_t expectedCount, std::chrono::milliseconds timeout) const
    {
        std::unique_lock<std::mutex> lock {mMutex};

        return mCondVar.wait_for(lock, timeout, [&]() { return mSendCount >= expectedCount; });
    }

private:
    mutable std::mutex              mMutex;
    mutable std::condition_variable mCondVar;
    OverrideEnvVarsStatuses         mStatuses;
    size_t                          mSendCount {};
};

} // namespace aos::cm::launcher

#endif
