/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "unitstatushandler.hpp"

namespace aos::cm::updatemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error UnitStatusHandler::Init(const Config& config, iamclient::IdentProviderItf& identProvider,
    unitconfig::UnitConfigItf& unitConfig, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    imagemanager::ImageStatusProviderItf& imageStatusProvider,
    instancestatusprovider::ProviderItf& instanceStatusProvider, SenderItf& sender)
{
    mIdentProvider          = &identProvider;
    mUnitConfig             = &unitConfig;
    mNodeInfoProvider       = &nodeInfoProvider;
    mImageStatusProvider    = &imageStatusProvider;
    mInstanceStatusProvider = &instanceStatusProvider;
    mSender                 = &sender;
    mUnitStatusSendTimeout  = config.mUnitStatusSendTimeout;

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::Start()
{
    LockGuard lock {mMutex};

    if (auto err = mNodeInfoProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mImageStatusProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInstanceStatusProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mIdentProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::Stop()
{
    LockGuard lock {mMutex};

    if (auto err = mNodeInfoProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mImageStatusProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInstanceStatusProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mIdentProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SendFullUnitStatus()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Send full unit status";

    mUnitStatus.mIsDeltaInfo = false;

    if (auto err = SetUnitConfigStatus(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetNodesInfo(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetUpdateItemsStatus(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetInstancesStatus(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetUnitSubjects(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LogUnitStatus();

    if (auto err = mSender->SendUnitStatus(mUnitStatus); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    ClearUnitStatus();

    mTimer.Stop();

    return ErrorEnum::eNone;
}

void UnitStatusHandler::SetCloudConnected(bool isConnected)
{
    LockGuard lock {mMutex};

    mCloudConnected = isConnected;

    if (!mCloudConnected) {
        mTimer.Stop();
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void UnitStatusHandler::OnNodeInfoChanged(const UnitNodeInfo& info)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Node info changed" << Log::Field("id", info.mNodeID) << Log::Field("type", info.mNodeType)
              << Log::Field("provisioned", info.mProvisioned) << Log::Field("state", info.mState)
              << Log::Field(info.mError);

    if (!mCloudConnected) {
        return;
    }

    if (!mUnitStatus.mNodes.HasValue()) {
        mUnitStatus.mNodes.EmplaceValue();
    }

    auto it = mUnitStatus.mNodes->FindIf(
        [&info](const UnitNodeInfo& nodeInfo) { return nodeInfo.mNodeID == info.mNodeID; });
    if (it != mUnitStatus.mNodes->end()) {
        *it = info;
    } else {
        if (auto err = mUnitStatus.mNodes->EmplaceBack(info); !err.IsNone()) {
            LOG_ERR() << "Failed to emplace node info" << Log::Field(err);
            return;
        }
    }

    StartTimer();
}

void UnitStatusHandler::OnImageStatusChanged(const String& itemID, const String& version, const ImageStatus& status)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Image status changed" << Log::Field("itemID", itemID) << Log::Field("version", version)
              << Log::Field("imageID", status.mImageID) << Log::Field("state", status.mState)
              << Log::Field(status.mError);

    if (!mCloudConnected) {
        return;
    }

    if (!mUnitStatus.mUpdateItems.HasValue()) {
        mUnitStatus.mUpdateItems.EmplaceValue();
    }

    auto itemIt = mUnitStatus.mUpdateItems->FindIf([&itemID, &version](const UpdateItemStatus& itemStatus) {
        return itemStatus.mItemID == itemID && itemStatus.mVersion == version;
    });
    if (itemIt == mUnitStatus.mUpdateItems->end()) {
        if (auto err = mUnitStatus.mUpdateItems->EmplaceBack(itemID, version); !err.IsNone()) {
            LOG_ERR() << "Failed to emplace update item status" << Log::Field(err);
            return;
        }

        itemIt = &mUnitStatus.mUpdateItems->Back();
    }

    auto statusIt = itemIt->mStatuses.FindIf(
        [&status](const ImageStatus& imageStatus) { return imageStatus.mImageID == status.mImageID; });
    if (statusIt != itemIt->mStatuses.end()) {
        *statusIt = status;
    } else {
        if (auto err = itemIt->mStatuses.EmplaceBack(status); !err.IsNone()) {
            LOG_ERR() << "Failed to emplace image status" << Log::Field(err);
            return;
        }
    }

    StartTimer();
}

void UnitStatusHandler::OnUpdateItemRemoved(const String& itemID)
{
    (void)itemID;
}

void UnitStatusHandler::OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses)
{
    LockGuard lock {mMutex};

    for (const auto& status : statuses) {
        LOG_INF() << "Instance status changed" << Log::Field("instance", static_cast<const InstanceIdent&>(status))
                  << Log::Field("version", status.mVersion) << Log::Field("nodeID", status.mNodeID)
                  << Log::Field("runtimeID", status.mRuntimeID) << Log::Field("imageID", status.mImageID)
                  << Log::Field("state", status.mState) << Log::Field(status.mError);
    }

    if (!mCloudConnected && statuses.Size() == 0) {
        return;
    }

    if (!mUnitStatus.mInstances.HasValue()) {
        mUnitStatus.mInstances.EmplaceValue();
    }

    for (const auto& status : statuses) {
        auto itemIt = mUnitStatus.mInstances->FindIf([&status](const UnitInstancesStatuses& instanceStatuses) {
            return instanceStatuses.mItemID == status.mItemID && instanceStatuses.mSubjectID == status.mSubjectID
                && instanceStatuses.mVersion == status.mVersion;
        });
        if (itemIt == mUnitStatus.mInstances->end()) {
            if (auto err = mUnitStatus.mInstances->EmplaceBack(status.mItemID, status.mSubjectID, status.mVersion);
                !err.IsNone()) {
                LOG_ERR() << "Failed to emplace instances statuses" << Log::Field(err);
                return;
            }

            itemIt = &mUnitStatus.mInstances->Back();
        }

        auto instanceIt = itemIt->mInstances.FindIf([&status](const UnitInstanceStatus& instanceStatus) {
            return instanceStatus.mInstance == status.mInstance;
        });
        if (instanceIt == itemIt->mInstances.end()) {
            if (auto err = itemIt->mInstances.EmplaceBack(); !err.IsNone()) {
                LOG_ERR() << "Failed to emplace instance status" << Log::Field(err);
                return;
            }

            instanceIt = &itemIt->mInstances.Back();
        }

        static_cast<InstanceStatusData&>(*instanceIt) = static_cast<const InstanceStatusData&>(status);
        instanceIt->mInstance                         = status.mInstance;
    }

    StartTimer();
}

void UnitStatusHandler::SubjectsChanged(const Array<StaticString<cIDLen>>& subjects)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Subject changed";

    for (const auto& subjectID : subjects) {
        LOG_INF() << "New subject" << Log::Field("subjectID", subjectID);
    }

    if (!mCloudConnected) {
        return;
    }

    if (!mUnitStatus.mUnitSubjects.HasValue()) {
        mUnitStatus.mUnitSubjects.EmplaceValue();
    } else {
        mUnitStatus.mUnitSubjects->Clear();
    }

    for (const auto& subjectID : subjects) {
        if (auto err = mUnitStatus.mUnitSubjects->EmplaceBack(subjectID); !err.IsNone()) {
            LOG_ERR() << "Failed to emplace subject" << Log::Field(err);
            return;
        }
    }

    StartTimer();
}

Error UnitStatusHandler::SetUnitConfigStatus()
{
    mUnitStatus.mUnitConfig.EmplaceValue();
    mUnitStatus.mUnitConfig->EmplaceBack();

    if (auto err = mUnitConfig->GetUnitConfigStatus(mUnitStatus.mUnitConfig.GetValue()[0]); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SetNodesInfo()
{
    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIDs;

    if (auto err = mNodeInfoProvider->GetAllNodeIDs(nodeIDs); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mUnitStatus.mNodes.EmplaceValue();
    mUnitStatus.mNodes->Resize(nodeIDs.Size());

    for (size_t i = 0; i < nodeIDs.Size(); i++) {
        auto& nodeInfo = mUnitStatus.mNodes.GetValue()[i];

        if (auto err = mNodeInfoProvider->GetNodeInfo(nodeIDs[i], nodeInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SetUpdateItemsStatus()
{
    mUnitStatus.mUpdateItems.EmplaceValue();

    mImageStatusProvider->GetUpdateItemsStatuses(*mUnitStatus.mUpdateItems);

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SetInstancesStatus()
{
    mUnitStatus.mInstances.EmplaceValue();

    auto instancesStatuses = MakeUnique<StaticArray<InstanceStatus, cMaxNumInstances>>(&mAllocator);

    if (auto err = mInstanceStatusProvider->GetInstancesStatuses(*instancesStatuses); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& status : *instancesStatuses) {
        auto it = mUnitStatus.mInstances->FindIf([&status](const UnitInstancesStatuses& instanceStatuses) {
            return instanceStatuses.mItemID == status.mItemID && instanceStatuses.mSubjectID == status.mSubjectID
                && instanceStatuses.mVersion == status.mVersion;
        });
        if (it == mUnitStatus.mInstances->end()) {
            if (auto err = mUnitStatus.mInstances->EmplaceBack(status.mItemID, status.mSubjectID, status.mVersion);
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            it = &mUnitStatus.mInstances->Back();
        }

        UnitInstanceStatus instanceStatus {};

        static_cast<InstanceStatusData&>(instanceStatus) = static_cast<const InstanceStatusData&>(status);
        instanceStatus.mInstance                         = status.mInstance;

        it->mInstances.PushBack(instanceStatus);
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SetUnitSubjects()
{
    mUnitStatus.mUnitSubjects.EmplaceValue();

    mIdentProvider->GetSubjects(*mUnitStatus.mUnitSubjects);

    return ErrorEnum::eNone;
}

void UnitStatusHandler::LogUnitStatus()
{
    if (mUnitStatus.mUnitConfig.HasValue()) {
        for (const auto& unitConfigStatus : mUnitStatus.mUnitConfig.GetValue()) {
            LOG_DBG() << "Unit config status" << Log::Field("version", unitConfigStatus.mVersion)
                      << Log::Field("state", unitConfigStatus.mState) << Log::Field(unitConfigStatus.mError);
        }
    }

    if (mUnitStatus.mNodes.HasValue()) {
        for (const auto& nodeInfo : mUnitStatus.mNodes.GetValue()) {
            LOG_DBG() << "Node info" << Log::Field("id", nodeInfo.mNodeID) << Log::Field("type", nodeInfo.mNodeType)
                      << Log::Field("provisioned", nodeInfo.mProvisioned) << Log::Field("state", nodeInfo.mState)
                      << Log::Field(nodeInfo.mError);
        }
    }

    if (mUnitStatus.mUpdateItems.HasValue()) {
        for (const auto& itemStatus : *mUnitStatus.mUpdateItems) {
            LOG_DBG() << "Update item status" << Log::Field("id", itemStatus.mItemID)
                      << Log::Field("version", itemStatus.mVersion);

            for (const auto& imageStatus : itemStatus.mStatuses) {
                LOG_DBG() << "Image status" << Log::Field("imageID", imageStatus.mImageID)
                          << Log::Field("state", imageStatus.mState) << Log::Field(imageStatus.mError);
            }
        }
    }

    if (mUnitStatus.mInstances.HasValue()) {
        for (const auto& instanceStatuses : *mUnitStatus.mInstances) {
            LOG_DBG() << "Instances statuses" << Log::Field("itemID", instanceStatuses.mItemID)
                      << Log::Field("subjectID", instanceStatuses.mSubjectID)
                      << Log::Field("version", instanceStatuses.mVersion);

            for (const auto& instanceStatus : instanceStatuses.mInstances) {
                LOG_DBG() << "Instance status" << Log::Field("instance", instanceStatus.mInstance)
                          << Log::Field("imageID", instanceStatus.mImageID)
                          << Log::Field("nodeID", instanceStatus.mNodeID)
                          << Log::Field("runtimeID", instanceStatus.mRuntimeID)
                          << Log::Field("state", instanceStatus.mState) << Log::Field(instanceStatus.mError);
            }
        }
    }

    if (mUnitStatus.mUnitSubjects.HasValue()) {
        for (const auto& subjectID : *mUnitStatus.mUnitSubjects) {
            LOG_DBG() << "Unit subject" << Log::Field("id", subjectID);
        }
    }
};

void UnitStatusHandler::ClearUnitStatus()
{
    mUnitStatus.mIsDeltaInfo = false;
    mUnitStatus.mUnitConfig.Reset();
    mUnitStatus.mNodes.Reset();
    mUnitStatus.mUpdateItems.Reset();
    mUnitStatus.mInstances.Reset();
    mUnitStatus.mUnitSubjects.Reset();
};

void UnitStatusHandler::StartTimer()
{
    if (mTimerStarted) {
        mTimer.Restart();

        return;
    }

    mTimerStarted = true;

    mTimer.Start(mUnitStatusSendTimeout, [this](void*) {
        LockGuard lock {mMutex};

        mUnitStatus.mIsDeltaInfo = true;

        LOG_DBG() << "Send delta unit status";

        LogUnitStatus();

        if (auto err = mSender->SendUnitStatus(mUnitStatus); !err.IsNone()) {
            LOG_ERR() << "Failed to send unit status" << Log::Field(err);
            return;
        }

        ClearUnitStatus();

        mTimerStarted = false;
    });
}

} // namespace aos::cm::updatemanager
