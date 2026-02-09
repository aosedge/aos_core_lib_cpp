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

Error DesiredStatusHandler::Init(iamclient::NodeHandlerItf& nodeHandler, unitconfig::UnitConfigItf& unitConfig,
    imagemanager::ImageManagerItf& imageManager, launcher::LauncherItf& launcher, UnitStatusHandler& unitStatusHandler,
    StorageItf& storage)
{
    LOG_DBG() << "Init desired status handler";

    mNodeHandler       = &nodeHandler;
    mUnitConfig        = &unitConfig;
    mUnitStatusHandler = &unitStatusHandler;
    mLauncher          = &launcher;
    mImageManager      = &imageManager;
    mStorage           = &storage;

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

    auto [updateState, err] = mStorage->GetUpdateState();
    if (!err.IsNone()) {
        LOG_ERR() << "Failed to get update state" << Log::Field(err);
    }

    if (updateState != UpdateStateEnum::eNone) {
        LOG_INF() << "Resuming update from state" << Log::Field("state", updateState);

        if (err = mStorage->GetDesiredStatus(mPendingDesiredStatus); !err.IsNone()) {
            LOG_ERR() << "Failed to get desired status" << Log::Field(err);
        } else {
            mHasPendingDesiredStatus = true;
            StartUpdate(updateState);
        }
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

        if (mUpdateState == UpdateStateEnum::eDownloading || mUpdateState == UpdateStateEnum::eInstalling) {
            if (auto cancelErr = mImageManager->Cancel(); !cancelErr.IsNone() && err.IsNone()) {
                err = AOS_ERROR_WRAP(cancelErr);
            }
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

    if (mUpdateState != UpdateStateEnum::eNone) {
        if (mPendingDesiredStatus == desiredStatus) {
            LOG_DBG() << "Desired status is already being processed";

            return ErrorEnum::eNone;
        }

        LOG_DBG() << "Cancel current update to process new desired status";

        CancelUpdate();
    } else {
        StartUpdate();
    }

    if (auto err = mStorage->StoreDesiredStatus(desiredStatus); !err.IsNone()) {
        LOG_ERR() << "Failed to store desired status" << Log::Field(err);
    }

    if (auto err = mStorage->StoreUpdateState(UpdateStateEnum::eDownloading); !err.IsNone()) {
        LOG_ERR() << "Failed to store update state" << Log::Field(err);
    }

    mPendingDesiredStatus    = desiredStatus;
    mHasPendingDesiredStatus = true;

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void DesiredStatusHandler::StartUpdate(UpdateState state)
{
    SetState(state);
    mCondVar.NotifyOne();
}

void DesiredStatusHandler::CancelUpdate()
{
    if (mCancelCurrentUpdate) {
        return;
    }

    mCancelCurrentUpdate = true;

    if (mUpdateState == UpdateStateEnum::eDownloading || mUpdateState == UpdateStateEnum::eInstalling) {
        if (auto err = mImageManager->Cancel(); !err.IsNone()) {
            LOG_ERR() << "Failed to cancel current update" << Log::Field(err);
        }
    }
}

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
                    stateAction = &DesiredStatusHandler::InstallDesiredStatus;
                    nextState   = UpdateStateEnum::eLaunching;

                    break;

                case UpdateStateEnum::eLaunching:
                    stateAction = &DesiredStatusHandler::LaunchInstances;
                    nextState   = UpdateStateEnum::eFinalizing;

                    break;

                case UpdateStateEnum::eFinalizing:
                    stateAction = &DesiredStatusHandler::FinalizeUpdate;
                    nextState   = UpdateStateEnum::eNone;

                    break;

                default:
                    break;
                }

                if (stateAction != nullptr) {
                    lock.Unlock();

                    auto err = (this->*stateAction)();

                    lock.Lock();

                    if (mCancelCurrentUpdate) {
                        break;
                    }

                    if (!err.IsNone()) {
                        LOG_ERR() << "Failed to process desired status" << Log::Field(err);

                        nextState = UpdateStateEnum::eNone;
                    }
                }

                SetState(nextState);
            }

            if (mCancelCurrentUpdate) {
                LOG_INF() << "Current update canceled";

                mCancelCurrentUpdate = false;

                SetState(UpdateStateEnum::eDownloading);

                continue;
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
        LOG_INF() << "Desired status node" << Log::Field("id", node.mNodeID) << Log::Field("state", node.mState);
    }

    if (desiredStatus.mUnitConfig.HasValue()) {
        LOG_INF() << "Desired status unit config update" << Log::Field("version", desiredStatus.mUnitConfig->mVersion);
    }

    for (const auto& item : desiredStatus.mUpdateItems) {
        LOG_INF() << "Desired status update item" << Log::Field("id", item.mItemID)
                  << Log::Field("version", item.mVersion);
    }

    for (const auto& instance : desiredStatus.mInstances) {
        LOG_INF() << "Desired status instance" << Log::Field("itemID", instance.mItemID)
                  << Log::Field("subjectID", instance.mSubjectID) << Log::Field("numInstances", instance.mNumInstances)
                  << Log::Field("priority", instance.mPriority);
    }

    for (const auto& subject : desiredStatus.mSubjects) {
        LOG_INF() << "Desired status subject" << Log::Field("id", subject.mSubjectID)
                  << Log::Field("type", subject.mSubjectType);
    }
}

void DesiredStatusHandler::SetState(UpdateState state)
{
    if (mUpdateState == state) {
        return;
    }

    LOG_INF() << "Update state changed" << Log::Field("state", state);

    if (auto err = mStorage->StoreUpdateState(state); !err.IsNone()) {
        LOG_ERR() << "Failed to store update state" << Log::Field(err);
    }

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

Error DesiredStatusHandler::InstallDesiredStatus()
{
    LOG_DBG() << "Install desired status";

    for (const auto& node : mCurrentDesiredStatus.mNodes) {
        LOG_DBG() << "Set node state" << Log::Field("id", node.mNodeID) << Log::Field("state", node.mState);

        Error updateErr;

        if (node.mState == DesiredNodeStateEnum::ePaused) {
            updateErr = mNodeHandler->PauseNode(node.mNodeID);
        } else {
            updateErr = mNodeHandler->ResumeNode(node.mNodeID);
        }

        if (!updateErr.IsNone()) {
            LOG_ERR() << "Failed to set node state" << Log::Field("id", node.mNodeID) << Log::Field(updateErr);

            if (auto err = mUnitStatusHandler->SetUpdateNodeStatus(node.mNodeID, updateErr); !err.IsNone()) {
                LOG_ERR() << "Failed to set update node status" << Log::Field("id", node.mNodeID) << Log::Field(err);
            }
        }
    }

    if (mCurrentDesiredStatus.mUnitConfig.HasValue()) {
        LOG_DBG() << "Update unit config" << Log::Field("version", mCurrentDesiredStatus.mUnitConfig->mVersion);

        auto updateErr = mUnitConfig->CheckUnitConfig(mCurrentDesiredStatus.mUnitConfig.GetValue());

        if (updateErr.IsNone()) {
            updateErr = mUnitConfig->UpdateUnitConfig(mCurrentDesiredStatus.mUnitConfig.GetValue());
        }

        if (!updateErr.IsNone()) {
            LOG_ERR() << "Failed to update unit config" << Log::Field(updateErr);
        }

        if (auto err = mUnitStatusHandler->SetUpdateUnitConfigStatus(
                UnitConfigStatus {mCurrentDesiredStatus.mUnitConfig->mVersion,
                    updateErr.IsNone() ? UnitConfigStateEnum::eInstalled : UnitConfigStateEnum::eFailed, updateErr});
            !err.IsNone()) {
            LOG_ERR() << "Failed to set unit config status" << Log::Field(err);
        }
    }

    return ErrorEnum::eNone;
}

Error DesiredStatusHandler::LaunchInstances()
{
    auto runRequest        = MakeUnique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>(&mAllocator);
    auto instancesStatuses = MakeUnique<StaticArray<InstanceStatus, cMaxNumInstances>>(&mAllocator);

    LOG_DBG() << "Launch instances" << Log::Field("count", mCurrentDesiredStatus.mInstances.Size());

    for (const auto& desiredInstance : mCurrentDesiredStatus.mInstances) {
        launcher::RunInstanceRequest request {};

        if (auto it = mCurrentDesiredStatus.mUpdateItems.FindIf(
                [&desiredInstance](const UpdateItemInfo& item) { return item.mItemID == desiredInstance.mItemID; });
            it != mCurrentDesiredStatus.mUpdateItems.end()) {
            request.mVersion        = it->mVersion;
            request.mOwnerID        = it->mOwnerID;
            request.mUpdateItemType = it->mType;
        } else {
            LOG_ERR() << "Update item for instance not found" << Log::Field("itemID", desiredInstance.mItemID);
        }

        if (auto it = mCurrentDesiredStatus.mSubjects.FindIf([&desiredInstance](const SubjectInfo& subject) {
                return subject.mSubjectID == desiredInstance.mSubjectID;
            });
            it != mCurrentDesiredStatus.mSubjects.end()) {
            request.mSubjectInfo = *it;
        } else {
            request.mSubjectInfo.mSubjectID = desiredInstance.mSubjectID;

            LOG_ERR() << "Subject for instance not found" << Log::Field("subjectID", desiredInstance.mSubjectID);
        }

        request.mItemID       = desiredInstance.mItemID;
        request.mPriority     = desiredInstance.mPriority;
        request.mNumInstances = desiredInstance.mNumInstances;
        request.mLabels       = desiredInstance.mLabels;

        if (auto err = runRequest->EmplaceBack(request); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = mLauncher->RunInstances(*runRequest, *instancesStatuses); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& instanceStatus : *instancesStatuses) {
        if (instanceStatus.mState == InstanceStateEnum::eFailed) {
            LOG_ERR() << "Failed to launch instance"
                      << Log::Field("item", static_cast<const InstanceIdent&>(instanceStatus))
                      << Log::Field("err", instanceStatus.mError);
        }
    }

    return ErrorEnum::eNone;
}

Error DesiredStatusHandler::FinalizeUpdate()
{
    auto itemsStatuses = MakeUnique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems>>(&mAllocator);

    LOG_DBG() << "Install update items" << Log::Field("count", mCurrentDesiredStatus.mUpdateItems.Size());

    if (auto err = mImageManager->InstallUpdateItems(mCurrentDesiredStatus.mUpdateItems, *itemsStatuses);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& itemStatus : *itemsStatuses) {
        if (itemStatus.mState == ItemStateEnum::eFailed) {
            LOG_ERR() << "Failed to install update item" << Log::Field("id", itemStatus.mItemID)
                      << Log::Field("version", itemStatus.mVersion) << Log::Field(itemStatus.mError);
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::updatemanager
