/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_TESTS_STUBS_SENDER_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_TESTS_STUBS_SENDER_HPP_

#include <condition_variable>
#include <mutex>

#include <core/cm/updatemanager/itf/sender.hpp>

namespace aos::cm::updatemanager {

/**
 * Sender stub.
 */
class SenderStub : public SenderItf {
public:
    /**
     * Sends unit status.
     *
     * @param unitStatus unit status.
     * @return Error.
     */
    Error SendUnitStatus(const UnitStatus& unitStatus) override
    {
        std::lock_guard lock {mMutex};

        mUnitStatus = unitStatus;
        mSendCalled = true;
        mCondVar.notify_one();

        return ErrorEnum::eNone;
    }

    const UnitStatus& WaitSendUnitStatus()
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, cWaitTimeout, [this] { return mSendCalled; })) {
            throw std::runtime_error("waiting for unit status timed out");
        }

        mSendCalled = false;

        return mUnitStatus;
    }

private:
    static constexpr std::chrono::milliseconds cWaitTimeout {5000};

    UnitStatus              mUnitStatus;
    bool                    mSendCalled {};
    std::mutex              mMutex;
    std::condition_variable mCondVar;
};

} // namespace aos::cm::updatemanager

#endif
