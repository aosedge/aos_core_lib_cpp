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

Error UnitStatusHandler::Init(iamclient::IdentProviderItf& identProvider, unitconfig::UnitConfigItf& unitConfig,
    nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider, imagemanager::ImageStatusProviderItf& imageStatusProvider,
    launcher::InstanceStatusProviderItf& instanceStatusProvider, SenderItf& sender)
{
    mIdentProvider          = &identProvider;
    mUnitConfig             = &unitConfig;
    mNodeInfoProvider       = &nodeInfoProvider;
    mImageStatusProvider    = &imageStatusProvider;
    mInstanceStatusProvider = &instanceStatusProvider;
    mSender                 = &sender;

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::Start()
{
    LockGuard lock {mMutex};

    mImageStatusProvider->SubscribeListener(*this);
    mInstanceStatusProvider->SubscribeListener(*this);

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::Stop()
{
    LockGuard lock {mMutex};

    mImageStatusProvider->UnsubscribeListener(*this);
    mInstanceStatusProvider->UnsubscribeListener(*this);

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

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void UnitStatusHandler::OnNodeInfoChanged(const UnitNodeInfo& info)
{
    (void)info;
}

void UnitStatusHandler::OnImageStatusChanged(const String& itemID, const String& version, const ImageStatus& status)
{
    (void)itemID;
    (void)version;
    (void)status;
}

void UnitStatusHandler::OnUpdateItemRemoved(const String& itemID)
{
    (void)itemID;
}

void UnitStatusHandler::OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses)
{
    (void)statuses;
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

            it = mUnitStatus.mInstances->end() - 1;
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

} // namespace aos::cm::updatemanager
