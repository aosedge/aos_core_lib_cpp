/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "offlinehandler.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error OfflineHandler::Start(cloudconnection::CloudConnectionItf& cloudConnection, OfflineCheckListenerItf& listener)
{
    mCloudConnection = &cloudConnection;
    mListener        = &listener;

    if (auto err = mCloudConnection->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!mCloudConnection->IsConnected()) {
        OnDisconnect();
    }

    return ErrorEnum::eNone;
}

Error OfflineHandler::Stop()
{
    Error stopErr;

    if (mCloudConnection != nullptr) {
        if (auto err = mCloudConnection->UnsubscribeListener(*this); !err.IsNone()) {
            stopErr = AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = mTimer.Stop(Timer::StopMode::WaitForCallbacks); !err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    return stopErr;
}

bool OfflineHandler::IsExpired(Duration offlineTTL) const
{
    LockGuard lock {mMutex};

    return mOfflineTime.HasValue() && offlineTTL && !(mOfflineTime->Add(offlineTTL) > Time::Now());
}

void OfflineHandler::Arm(const Optional<Duration>& minOfflineTTL)
{
    LockGuard lock {mMutex};

    if (!mOfflineTime.HasValue() || !minOfflineTTL.HasValue()) {
        return;
    }

    // The timer doesn't accept the interval below its resolution.
    const auto deadline  = mOfflineTime->Add(*minOfflineTTL);
    const auto now       = Time::Now();
    const auto remaining = deadline > now.Add(Time::cMilliseconds) ? deadline.Sub(now) : Time::cMilliseconds;

    if (auto err = mTimer.Start(remaining, [this](void*) { mListener->OnOfflineCheck(); }); !err.IsNone()) {
        LOG_ERR() << "Can't start offline TTL timer" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void OfflineHandler::OnConnect()
{
    LOG_DBG() << "Cloud connected event";

    LockGuard lock {mMutex};

    mOfflineTime.Reset();

    if (auto err = mTimer.Stop(); !err.IsNone()) {
        LOG_ERR() << "Can't stop offline TTL timer" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

void OfflineHandler::OnDisconnect()
{
    LOG_DBG() << "Cloud disconnected event";

    {
        LockGuard lock {mMutex};

        mOfflineTime = Time::Now();
    }

    mListener->OnOfflineCheck();
}

} // namespace aos::sm::launcher
