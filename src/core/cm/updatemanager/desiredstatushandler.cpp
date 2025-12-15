/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "desiredstatushandler.hpp"

namespace aos::cm::updatemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error DesiredStatusHandler::Init(UnitStatusHandler& unitStatusHandler, imagemanager::ImageManagerItf& imageManager)
{
    LOG_DBG() << "Init desired status handler";

    mUnitStatusHandler = &unitStatusHandler;
    mImageManager      = &imageManager;

    return ErrorEnum::eNone;
}

Error DesiredStatusHandler::Start()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start desired status handler";

    if (mIsRunning) {
        return ErrorEnum::eWrongState;
    }

    mIsRunning = true;

    if (auto err = mThread.Run([this](void*) { Run(); }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error DesiredStatusHandler::Stop()
{
    Error err;

    LOG_DBG() << "Stop desired status handler";

    {
        LockGuard lock {mMutex};

        if (!mIsRunning) {
            return ErrorEnum::eWrongState;
        }

        mIsRunning = false;
        mCondVar.NotifyOne();
    }

    if (auto threadErr = mThread.Join(); !threadErr.IsNone() && err.IsNone()) {
        err = AOS_ERROR_WRAP(threadErr);
    }

    return err;
}

Error DesiredStatusHandler::ProcessDesiredStatus(const DesiredStatus& desiredStatus)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Process desired status";

    LogDesiredStatus(desiredStatus);

    mPendingDesiredStatus    = desiredStatus;
    mHasPendingDesiredStatus = true;

    if (mUpdateState != UpdateStateEnum::eNone) {
        LOG_WRN() << "Desired status processing is already scheduled. Set new desired status as pending";
    } else {
        SetState(UpdateStateEnum::eDownloading);
        mCondVar.NotifyOne();
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void DesiredStatusHandler::Run()
{
    while (true) {
        {
            UniqueLock lock {mMutex};

            if (auto err
                = mCondVar.Wait(lock, [this]() { return !mIsRunning || mUpdateState != UpdateStateEnum::eNone; });
                !err.IsNone()) {
                LOG_ERR() << "Error waiting cond var" << Log::Field(err);
            }

            if (!mIsRunning) {
                return;
            }

            mCurrentDesiredStatus    = mPendingDesiredStatus;
            mHasPendingDesiredStatus = false;

            while (mUpdateState != UpdateStateEnum::eNone && mIsRunning) {
                Error (DesiredStatusHandler::*stateAction)() = nullptr;
                UpdateState nextState                        = UpdateStateEnum::eNone;

                switch (mUpdateState.GetValue()) {
                case UpdateStateEnum::eDownloading:
                    stateAction = &DesiredStatusHandler::DownloadUpdateItems;
                    nextState   = UpdateStateEnum::ePending;

                    break;

                case UpdateStateEnum::ePending:
                    stateAction = nullptr;
                    nextState   = UpdateStateEnum::eInstalling;

                    break;

                case UpdateStateEnum::eInstalling:
                    stateAction = nullptr;
                    nextState   = UpdateStateEnum::eLaunching;

                    break;

                case UpdateStateEnum::eLaunching:
                    stateAction = nullptr;
                    nextState   = UpdateStateEnum::eFinalizing;

                    break;

                case UpdateStateEnum::eFinalizing:
                    stateAction = nullptr;
                    nextState   = UpdateStateEnum::eNone;

                    break;

                default:
                    break;
                }

                if (stateAction != nullptr) {
                    lock.Unlock();

                    if (auto err = (this->*stateAction)(); !err.IsNone()) {
                        LOG_ERR() << "Failed to process desired status" << Log::Field(err);

                        nextState = UpdateStateEnum::eNone;
                    }

                    lock.Lock();
                }

                SetState(nextState);
            }

            mUnitStatusHandler->SendFullUnitStatus();

            if (mHasPendingDesiredStatus) {
                LOG_DBG() << "Process pending desired status";

                SetState(UpdateStateEnum::eDownloading);

                continue;
            }
        }
    }
}

void DesiredStatusHandler::LogDesiredStatus(const DesiredStatus& desiredStatus)
{
    for (const auto& node : desiredStatus.mNodes) {
        LOG_DBG() << "Node" << Log::Field("id", node.mNodeID) << Log::Field("state", node.mState);
    }

    if (desiredStatus.mUnitConfig.HasValue()) {
        LOG_DBG() << "Unit config update" << Log::Field("version", desiredStatus.mUnitConfig->mVersion);
    }

    for (const auto& item : desiredStatus.mUpdateItems) {
        LOG_DBG() << "Update item" << Log::Field("id", item.mItemID) << Log::Field("version", item.mVersion);
    }

    for (const auto& instance : desiredStatus.mInstances) {
        LOG_DBG() << "Instance" << Log::Field("itemID", instance.mItemID)
                  << Log::Field("subjectID", instance.mSubjectID) << Log::Field("numInstances", instance.mNumInstances)
                  << Log::Field("priority", instance.mPriority);
    }

    for (const auto& subject : desiredStatus.mSubjects) {
        LOG_DBG() << "Subject" << Log::Field("id", subject.mSubjectID) << Log::Field("type", subject.mSubjectType);
    }
}

void DesiredStatusHandler::SetState(UpdateState state)
{
    if (mUpdateState == state) {
        return;
    }

    LOG_DBG() << "Update state changed" << Log::Field("state", state);

    mUpdateState = state;
}

Error DesiredStatusHandler::DownloadUpdateItems()
{
    auto itemsStatuses = MakeUnique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems>>(&mAllocator);

    LOG_DBG() << "Download update items" << Log::Field("count", mCurrentDesiredStatus.mUpdateItems.Size());

    if (auto err = mImageManager->DownloadUpdateItems(mCurrentDesiredStatus.mUpdateItems,
            mCurrentDesiredStatus.mCertificates, mCurrentDesiredStatus.mCertificateChains, *itemsStatuses);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& itemStatus : *itemsStatuses) {
        if (itemStatus.mState == ItemStateEnum::eFailed) {
            LOG_ERR() << "Failed to download update item" << Log::Field("id", itemStatus.mItemID)
                      << Log::Field("version", itemStatus.mVersion) << Log::Field(itemStatus.mError);
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::updatemanager
