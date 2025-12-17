/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "alerts.hpp"

namespace aos::cm::alerts {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

class GetTimestamp : public StaticVisitor<Time> {
public:
    Res Visit(const AlertItem& alert) const { return alert.mTimestamp; }
};

class SetTimestamp : public StaticVisitor<void> {
public:
    explicit SetTimestamp(const Time& time)
        : mTime(time)
    {
    }

    template <typename T>
    Res Visit(T& val) const
    {
        val.mTimestamp = mTime;
    }

private:
    Time mTime;
};

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Alerts::Init(const alerts::Config& config, cm::alerts::SenderItf& sender)
{
    LOG_DBG() << "Initialize alerts";

    mConfig = config;
    mSender = &sender;

    return ErrorEnum::eNone;
}

Error Alerts::Start()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start alerts module";

    if (mIsRunning) {
        return ErrorEnum::eWrongState;
    }

    mIsRunning = true;

    return mSendTimer.Start(
        mConfig.mSendPeriod,
        [this](void*) {
            if (auto err = SendAlerts(); !err.IsNone()) {
                LOG_ERR() << "Failed to send alerts" << Log::Field(err);
            }
        },
        false);
}

Error Alerts::Stop()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Stop alerts module";

    if (!mIsRunning) {
        return ErrorEnum::eWrongState;
    }

    mIsRunning = false;

    return mSendTimer.Stop();
}

Error Alerts::OnAlertReceived(const AlertVariant& alert)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Alert received" << Log::Field("alert", alert);

    return HandleAlert(alert);
}

void Alerts::OnConnect()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Publisher connected";

    mIsConnected = true;
}

void Alerts::OnDisconnect()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Publisher disconnected";

    mIsConnected = false;
}

Error Alerts::SendAlert(const AlertVariant& alert)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Send alert" << Log::Field("alert", alert);

    return HandleAlert(alert);
}

Error Alerts::SubscribeListener(const Array<AlertTag>& tags, AlertsListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe listener" << Log::Field("tagsCount", tags.Size());

    for (const auto& tag : tags) {
        if (auto err = mListeners.TryEmplace(tag); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto it = mListeners.Find(tag);
        if (it == mListeners.end()) {
            return AOS_ERROR_WRAP(ErrorEnum::eFailed);
        }

        auto& listeners = it->mSecond;
        if (listeners.Contains(&listener)) {
            continue;
        }

        if (auto err = listeners.EmplaceBack(&listener); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Alerts::UnsubscribeListener(AlertsListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unsubscribe listener";

    size_t removed = 0;

    for (auto& [tag, listeners] : mListeners) {
        removed += listeners.Remove(&listener);
    }

    return removed > 0 ? ErrorEnum::eNone : ErrorEnum::eNotFound;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Alerts::HandleAlert(const AlertVariant& alert)
{
    NotifyListeners(alert);

    if (IsDuplicated(alert)) {
        ++mDuplicatedAlerts;

        return ErrorEnum::eNone;
    }

    if (auto err = mAlerts.EmplaceBack(alert); !err.IsNone()) {
        ++mSkippedAlerts;

        if (!err.Is(ErrorEnum::eNoMemory)) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error Alerts::SendAlerts()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Send alerts timer triggered";

    if (!mIsRunning || !mIsConnected || mAlerts.IsEmpty()) {
        return ErrorEnum::eNone;
    }

    if (mSkippedAlerts > 0) {
        LOG_WRN() << "Alerts skipped due to cache is full" << Log::Field("count", mSkippedAlerts);

        mSkippedAlerts = 0;
    }

    if (mDuplicatedAlerts > 0) {
        LOG_WRN() << "Alerts skipped due to duplication" << Log::Field("count", mDuplicatedAlerts);

        mDuplicatedAlerts = 0;
    }

    while (!mAlerts.IsEmpty()) {
        auto package = CreatePackage();

        if (auto err = mSender->SendAlerts(*package); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ShrinkCache(package->mItems.Size());
    }

    return ErrorEnum::eNone;
}

bool Alerts::IsDuplicated(const AlertVariant& alert)
{
    auto alertCopy = MakeUnique<AlertVariant>(&mAllocator, alert);

    return mAlerts.FindIf([&alertCopy](const AlertVariant& item) {
        alertCopy->ApplyVisitor(SetTimestamp(item.ApplyVisitor(GetTimestamp())));

        return *alertCopy == item;
    }) != mAlerts.end();
}

UniquePtr<aos::Alerts> Alerts::CreatePackage()
{
    auto package = MakeUnique<aos::Alerts>(&mAllocator);

    const auto count = Min<size_t>(cAlertItemsCount, mAlerts.Size());

    package->mItems.Assign(Array<AlertVariant>(mAlerts.begin(), count));

    return package;
}

void Alerts::ShrinkCache(size_t count)
{
    mAlerts.Erase(mAlerts.begin(), mAlerts.begin() + Min<size_t>(count, mAlerts.Size()));
}

void Alerts::NotifyListeners(const AlertVariant& alert)
{
    const auto tag = alert.ApplyVisitor(GetAlertTagVisitor());

    if (auto it = mListeners.Find(tag); it != mListeners.end()) {
        for (auto* receiver : it->mSecond) {
            if (!receiver) {
                continue;
            }

            receiver->OnAlertReceived(alert);
        }
    }
}

} // namespace aos::cm::alerts
