/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "updatemanager.hpp"

namespace aos::cm::updatemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error UpdateManager::Init(const Config& config, iamclient::IdentProviderItf& identProvider,
    unitconfig::UnitConfigItf& unitConfig, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    imagemanager::ImageManagerItf& imageManager, instancestatusprovider::ProviderItf& instanceStatusProvider,
    cloudconnection::CloudConnectionItf& cloudConnection, SenderItf& sender)
{
    LOG_DBG() << "Init update manager";

    mCloudConnection = &cloudConnection;

    if (auto err = mUnitStatusHandler.Init(
            config, identProvider, unitConfig, nodeInfoProvider, imageManager, instanceStatusProvider, sender);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UpdateManager::Start()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start update manager";

    if (mIsRunning) {
        return ErrorEnum::eWrongState;
    }

    if (auto err = mUnitStatusHandler.Start(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mIsRunning = true;

    mCloudConnection->SubscribeListener(*this);

    if (auto err = mThread.Run([this](void*) { Run(); }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UpdateManager::Stop()
{
    Error err;

    {
        LockGuard lock {mMutex};

        LOG_DBG() << "Stop update manager";

        if (!mIsRunning) {
            return ErrorEnum::eWrongState;
        }

        if (auto unitStatusErr = mUnitStatusHandler.Stop(); !unitStatusErr.IsNone() && err.IsNone()) {
            err = AOS_ERROR_WRAP(unitStatusErr);
        }

        mIsRunning = false;
        mCloudConnection->UnsubscribeListener(*this);
        mCondVar.NotifyOne();
    }

    if (auto threadErr = mThread.Join(); !threadErr.IsNone() && err.IsNone()) {
        err = AOS_ERROR_WRAP(threadErr);
    }

    return err;
}

Error UpdateManager::ProcessDesiredStatus(const DesiredStatus& desiredStatus)
{
    (void)desiredStatus;

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void UpdateManager::OnConnect()
{
    LockGuard lock {mMutex};

    mUnitStatusHandler.SetCloudConnected(true);

    mSendUnitStatus = true;
    mCondVar.NotifyOne();
}

void UpdateManager::OnDisconnect()
{
    LockGuard lock {mMutex};

    mUnitStatusHandler.SetCloudConnected(false);
}

void UpdateManager::Run()
{
    while (true) {
        UniqueLock lock {mMutex};

        if (auto err = mCondVar.Wait(lock, [this]() { return !mIsRunning || mSendUnitStatus; }); !err.IsNone()) {
            LOG_ERR() << "Error waiting cond var" << Log::Field(err);
        }

        if (!mIsRunning) {
            return;
        }

        if (mSendUnitStatus) {
            mSendUnitStatus = false;

            if (auto err = mUnitStatusHandler.SendFullUnitStatus(); !err.IsNone()) {
                LOG_ERR() << "Error send full unit status" << Log::Field(err);
            }
        }
    }
}

} // namespace aos::cm::updatemanager
