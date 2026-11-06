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

Error UnitStatusHandler::Init(AllocatorItf& allocator, const Config& config, iamclient::IdentProviderItf& identProvider,
    unitconfig::UnitConfigItf& unitConfig, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    imagemanager::ItemStatusProviderItf& itemStatusProvider,
    instancestatusprovider::ProviderItf& instanceStatusProvider, cloudconnection::CloudConnectionItf& cloudConnection,
    SenderItf& sender)
{
    mAllocator              = &allocator;
    mIdentProvider          = &identProvider;
    mUnitConfig             = &unitConfig;
    mNodeInfoProvider       = &nodeInfoProvider;
    mItemStatusProvider     = &itemStatusProvider;
    mInstanceStatusProvider = &instanceStatusProvider;
    mCloudConnection        = &cloudConnection;
    mSender                 = &sender;
    mUnitStatusSendTimeout  = config.mUnitStatusSendTimeout;

    LOG_DBG() << "Init unit status handler";

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::Start()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start unit status handler";

    if (auto err = mNodeInfoProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mItemStatusProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInstanceStatusProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mIdentProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mCloudConnection->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::Stop()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Stop unit status handler";

    if (auto err = mNodeInfoProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mItemStatusProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInstanceStatusProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mIdentProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mCloudConnection->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SendFullUnitStatus()
{
    {
        LockGuard lock {mMutex};

        if (!mCloudConnected) {
            return ErrorEnum::eNone;
        }

        mIsStatusProcessing = true;
    }

    LOG_INF() << "[profiling] Send full unit status";

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

    if (auto err = SetItemsForPreinstalledInstances(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetItemsForFailedInstances(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    {
        LogUnitStatus();

        if (auto err = mSender->SendUnitStatus(mUnitStatus); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        ClearUnitStatus();
        ClearUpdateStatuses();

        if (auto err = mTimer.Stop(); !err.IsNone()) {
            LOG_ERR() << "Can't stop unit status timer" << Log::Field(AOS_ERROR_WRAP(err));
        }

        LockGuard lock {mMutex};

        mIsStatusProcessing = false;
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SetUpdateUnitConfigStatus(const UnitConfigStatus& status)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Unit config status changed" << Log::Field("version", status.mVersion)
              << Log::Field("state", status.mState) << Log::Field(status.mError);

    mUpdateUnitConfigStatus = status;

    if (!mCloudConnected) {
        return ErrorEnum::eNone;
    }

    if (!mUnitStatus.mUnitConfig.HasValue()) {
        mUnitStatus.mUnitConfig.EmplaceValue();
    } else {
        mUnitStatus.mUnitConfig->Clear();
    }

    if (auto err = mUnitStatus.mUnitConfig->EmplaceBack(status); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StartTimer();

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SetUpdateNodeStatus(const String& nodeID, const Error& updateErr)
{
    LockGuard lock {mMutex};

    LOG_INF() << "Node update status changed" << Log::Field("nodeID", nodeID) << Log::Field(updateErr);

    if (auto err = mUpdateNodeStatuses.Set(nodeID, updateErr); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void UnitStatusHandler::OnNodeInfoChanged(const UnitNodeInfo& info)
{
    LockGuard lock {mMutex};

    auto nodeError = info.mError;

    if (nodeError.IsNone()) {
        auto updateNodeStatusIt = mUpdateNodeStatuses.Find(info.mNodeID);
        if (updateNodeStatusIt != mUpdateNodeStatuses.end()) {
            nodeError = updateNodeStatusIt->mSecond;
        }
    }

    LOG_DBG() << "Node info changed" << Log::Field("id", info.mNodeID) << Log::Field("type", info.mNodeType)
              << Log::Field("state", info.mState) << Log::Field("isConnected", info.mIsConnected)
              << Log::Field(nodeError);

    if (!mCloudConnected || mIsStatusProcessing) {
        return;
    }

    if (!mUnitStatus.mNodes.HasValue()) {
        mUnitStatus.mNodes.EmplaceValue();
    }

    if (auto it = mUnitStatus.mNodes->FindIf(
            [&info](const UnitNodeInfo& nodeInfo) { return nodeInfo.mNodeID == info.mNodeID; });
        it != mUnitStatus.mNodes->end()) {
        *it = info;
    } else {
        if (auto err = mUnitStatus.mNodes->EmplaceBack(info); !err.IsNone()) {
            LOG_ERR() << "Failed to emplace node info" << Log::Field(err);
            return;
        }
    }

    StartTimer();
}

void UnitStatusHandler::OnItemsStatusesChanged(const Array<UpdateItemStatus>& statuses)
{
    LockGuard lock {mMutex};

    for (const auto& status : statuses) {
        LOG_DBG() << "Item status changed" << Log::Field("id", status.mItemID) << Log::Field("type", status.mType)
                  << Log::Field("version", status.mVersion) << Log::Field("state", status.mState)
                  << Log::Field(status.mError);
    }

    if (!mCloudConnected || mIsStatusProcessing) {
        return;
    }

    if (!mUnitStatus.mUpdateItems.HasValue() && !statuses.IsEmpty()) {
        mUnitStatus.mUpdateItems.EmplaceValue();
    }

    for (const auto& status : statuses) {
        auto it = mUnitStatus.mUpdateItems->FindIf([&status](const UpdateItemStatus& itemStatus) {
            return itemStatus.mItemID == status.mItemID && itemStatus.mVersion == status.mVersion;
        });
        if (it == mUnitStatus.mUpdateItems->end()) {
            if (auto err = mUnitStatus.mUpdateItems->EmplaceBack(status); !err.IsNone()) {
                LOG_ERR() << "Failed to emplace update item status" << Log::Field(err);
                return;
            }
        }

        *it = status;
    }

    StartTimer();
}

void UnitStatusHandler::OnItemRemoved(const String& itemID)
{
    (void)itemID;
}

void UnitStatusHandler::OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses)
{
    LockGuard lock {mMutex};

    for (const auto& status : statuses) {
        LOG_DBG() << "Instance status changed" << Log::Field("instance", static_cast<const InstanceIdent&>(status))
                  << Log::Field("version", status.mVersion) << Log::Field("nodeID", status.mNodeID)
                  << Log::Field("runtimeID", status.mRuntimeID) << Log::Field("manifestDigest", status.mManifestDigest)
                  << Log::Field("state", status.mState) << Log::Field(status.mError);
    }

    if (!mCloudConnected || mIsStatusProcessing || statuses.IsEmpty()) {
        return;
    }

    if (!mUnitStatus.mInstances.HasValue()) {
        mUnitStatus.mInstances.EmplaceValue();
    }

    for (const auto& status : statuses) {
        if (auto err = UpdateItemInstance(status); !err.IsNone()) {
            LOG_ERR() << "Failed to update item instance" << Log::Field(err);
        }

        auto itemIt = mUnitStatus.mInstances->FindIf([&status](const UnitInstancesStatuses& instanceStatuses) {
            return instanceStatuses.mItemID == status.mItemID && instanceStatuses.mSubjectID == status.mSubjectID
                && instanceStatuses.mVersion == status.mVersion;
        });
        if (itemIt == mUnitStatus.mInstances->end()) {
            if (auto err = mUnitStatus.mInstances->EmplaceBack(
                    status.mItemID, status.mType, status.mSubjectID, status.mVersion, status.mPreinstalled);
                !err.IsNone()) {
                LOG_ERR() << "Failed to emplace instances statuses" << Log::Field(err);
                return;
            }

            itemIt = &mUnitStatus.mInstances->Back();
        }

        auto instanceIt = itemIt->mInstances.FindIf([&status](const UnitInstanceStatus* instanceStatus) {
            return instanceStatus->mInstance == status.mInstance;
        });
        if (instanceIt == itemIt->mInstances.end()) {
            if (auto err = mUnitInstancesStatuses.EmplaceBack(); !err.IsNone()) {
                LOG_ERR() << "Failed to emplace instance status" << Log::Field(err);
                return;
            }

            if (auto err = itemIt->mInstances.PushBack(&mUnitInstancesStatuses.Back()); !err.IsNone()) {
                LOG_ERR() << "Failed to push instance status pointer" << Log::Field(err);
                return;
            }

            instanceIt = &itemIt->mInstances.Back();
        }

        static_cast<InstanceStatusData&>(**instanceIt) = static_cast<const InstanceStatusData&>(status);
        (*instanceIt)->mInstance                       = status.mInstance;
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

    if (!mCloudConnected || mIsStatusProcessing) {
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

void UnitStatusHandler::OnConnect()
{
    {
        LockGuard lock {mMutex};

        LOG_DBG() << "Cloud connected";

        mCloudConnected = true;

        if (mIsStatusProcessing) {
            return;
        }
    }

    if (auto err = SendFullUnitStatus(); !err.IsNone()) {
        LOG_ERR() << "Failed to send full unit status on cloud connect" << Log::Field(err);
    }
}

void UnitStatusHandler::OnDisconnect()
{
    LockGuard lock {mMutex};

    mCloudConnected = false;

    if (auto err = mTimer.Stop(); !err.IsNone()) {
        LOG_ERR() << "Can't stop unit status timer" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

Error UnitStatusHandler::SetUnitConfigStatus()
{
    mUnitStatus.mUnitConfig.EmplaceValue();

    if (auto err = mUnitStatus.mUnitConfig->EmplaceBack(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto& unitConfigStatus = mUnitStatus.mUnitConfig->Back();

    if (auto err = mUnitConfig->GetUnitConfigStatus(unitConfigStatus); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (mUpdateUnitConfigStatus.HasValue() && mUpdateUnitConfigStatus->mVersion != unitConfigStatus.mVersion) {
        if (auto err = mUnitStatus.mUnitConfig->EmplaceBack(mUpdateUnitConfigStatus.GetValue()); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
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
    (void)mUnitStatus.mNodes->Resize(nodeIDs.Size());

    for (size_t i = 0; i < nodeIDs.Size(); i++) {
        auto& nodeInfo = mUnitStatus.mNodes.GetValue()[i];

        if (auto err = mNodeInfoProvider->GetNodeInfo(nodeIDs[i], nodeInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (nodeInfo.mError.IsNone()) {
            auto updateNodeStatusIt = mUpdateNodeStatuses.Find(nodeInfo.mNodeID);
            if (updateNodeStatusIt != mUpdateNodeStatuses.end()) {
                nodeInfo.mError = updateNodeStatusIt->mSecond;
            }
        }
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SetUpdateItemsStatus()
{
    auto itemsStatuses = MakeUnique<UpdateItemStatusArray>(mAllocator);
    if (!itemsStatuses) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mItemStatusProvider->GetUpdateItemsStatuses(*itemsStatuses); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mUnitStatus.mUpdateItems.EmplaceValue();

    for (const auto& status : *itemsStatuses) {
        if (status.mState == ItemStateEnum::eRemoved) {
            continue;
        }

        if (auto err = mUnitStatus.mUpdateItems->EmplaceBack(status); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SetInstancesStatus()
{
    mUnitStatus.mInstances.EmplaceValue();
    mUnitInstancesStatuses.Clear();
    mItemInstances.Clear();

    auto instancesStatuses = MakeUnique<StaticArray<InstanceStatus, cMaxNumInstances>>(mAllocator);
    if (!instancesStatuses) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = mInstanceStatusProvider->GetInstancesStatuses(*instancesStatuses); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& status : *instancesStatuses) {
        if (auto err = UpdateItemInstance(status); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto it = mUnitStatus.mInstances->FindIf([&status](const UnitInstancesStatuses& instanceStatuses) {
            return instanceStatuses.mItemID == status.mItemID && instanceStatuses.mSubjectID == status.mSubjectID
                && instanceStatuses.mVersion == status.mVersion;
        });
        if (it == mUnitStatus.mInstances->end()) {
            if (auto err = mUnitStatus.mInstances->EmplaceBack(
                    status.mItemID, status.mType, status.mSubjectID, status.mVersion, status.mPreinstalled);
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            it = &mUnitStatus.mInstances->Back();
        }

        if (auto err = mUnitInstancesStatuses.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& instanceStatus = mUnitInstancesStatuses.Back();

        static_cast<InstanceStatusData&>(instanceStatus) = static_cast<const InstanceStatusData&>(status);
        instanceStatus.mInstance                         = status.mInstance;

        (void)it->mInstances.PushBack(&instanceStatus);
    }

    return ErrorEnum::eNone;
}

Error UnitStatusHandler::SetUnitSubjects()
{
    mUnitStatus.mUnitSubjects.EmplaceValue();

    if (auto err = mIdentProvider->GetSubjects(*mUnitStatus.mUnitSubjects); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void UnitStatusHandler::LogUnitStatus()
{
    if (mUnitStatus.mUnitConfig.HasValue()) {
        for (const auto& unitConfigStatus : mUnitStatus.mUnitConfig.GetValue()) {
            LOG_INF() << "Unit status unit config" << Log::Field("version", unitConfigStatus.mVersion)
                      << Log::Field("state", unitConfigStatus.mState) << Log::Field(unitConfigStatus.mError);
        }
    }

    if (mUnitStatus.mNodes.HasValue()) {
        for (const auto& nodeInfo : mUnitStatus.mNodes.GetValue()) {
            LOG_INF() << "Unit status node info" << Log::Field("id", nodeInfo.mNodeID)
                      << Log::Field("type", nodeInfo.mNodeType) << Log::Field("isConnected", nodeInfo.mIsConnected)
                      << Log::Field("state", nodeInfo.mState) << Log::Field(nodeInfo.mError);
        }
    }

    if (mUnitStatus.mUpdateItems.HasValue()) {
        for (const auto& itemStatus : *mUnitStatus.mUpdateItems) {
            LOG_INF() << "Unit status update item" << Log::Field("id", itemStatus.mItemID)
                      << Log::Field("type", itemStatus.mType) << Log::Field("version", itemStatus.mVersion)
                      << Log::Field("state", itemStatus.mState) << Log::Field(itemStatus.mError);
        }
    }

    if (mUnitStatus.mInstances.HasValue()) {
        for (const auto& instanceStatuses : *mUnitStatus.mInstances) {
            LOG_INF() << "Unit status instances" << Log::Field("itemID", instanceStatuses.mItemID)
                      << Log::Field("subjectID", instanceStatuses.mSubjectID)
                      << Log::Field("version", instanceStatuses.mVersion);

            for (const auto& instanceStatus : instanceStatuses.mInstances) {
                LOG_INF() << "Unit status instance" << Log::Field("instance", instanceStatus->mInstance)
                          << Log::Field("manifestDigest", instanceStatus->mManifestDigest)
                          << Log::Field("nodeID", instanceStatus->mNodeID)
                          << Log::Field("runtimeID", instanceStatus->mRuntimeID)
                          << Log::Field("state", instanceStatus->mState) << Log::Field(instanceStatus->mError);
            }
        }
    }

    if (mUnitStatus.mUnitSubjects.HasValue()) {
        for (const auto& subjectID : *mUnitStatus.mUnitSubjects) {
            LOG_INF() << "Unit status unit subject" << Log::Field("id", subjectID);
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
    mUnitInstancesStatuses.Clear();
};

void UnitStatusHandler::ClearUpdateStatuses()
{
    mUpdateUnitConfigStatus.Reset();
    mUpdateNodeStatuses.Clear();
}

void UnitStatusHandler::StartTimer()
{
    if (mTimerStarted) {
        if (auto err = mTimer.Restart(); !err.IsNone()) {
            LOG_ERR() << "Can't restart unit status timer" << Log::Field(AOS_ERROR_WRAP(err));
        }

        return;
    }

    mTimerStarted = true;

    if (auto err = mTimer.Start(mUnitStatusSendTimeout,
            [this](void*) {
                LockGuard lock {mMutex};

                mUnitStatus.mIsDeltaInfo = true;

                LOG_INF() << "Send delta unit status";

                if (auto err = SetItemsForPreinstalledInstances(); !err.IsNone()) {
                    LOG_ERR() << "Failed to set items for preinstalled instances" << Log::Field(err);
                }

                if (auto err = SetItemsForFailedInstances(); !err.IsNone()) {
                    LOG_ERR() << "Failed to set items for failed instances" << Log::Field(err);
                }

                LogUnitStatus();

                if (auto err = mSender->SendUnitStatus(mUnitStatus); !err.IsNone()) {
                    LOG_ERR() << "Failed to send unit status" << Log::Field(err);
                }

                ClearUnitStatus();

                mTimerStarted = false;
            });
        !err.IsNone()) {
        mTimerStarted = false;
        LOG_ERR() << "Can't start unit status timer" << Log::Field(AOS_ERROR_WRAP(err));
    }
}

Error UnitStatusHandler::SetItemsForPreinstalledInstances()
{
    if (!mUnitStatus.mInstances.HasValue()) {
        return ErrorEnum::eNone;
    }

    for (const auto& instanceStatuses : *mUnitStatus.mInstances) {
        if (!instanceStatuses.mPreinstalled) {
            continue;
        }

        if (!mUnitStatus.mUpdateItems.HasValue()) {
            mUnitStatus.mUpdateItems.EmplaceValue();
        }

        auto itemIt = mUnitStatus.mUpdateItems->FindIf([&instanceStatuses](const UpdateItemStatus& itemStatus) {
            return itemStatus.mItemID == instanceStatuses.mItemID && itemStatus.mVersion == instanceStatuses.mVersion;
        });
        if (itemIt != mUnitStatus.mUpdateItems->end()) {
            itemIt->mPreinstalled = true;
        } else {
            LOG_DBG() << "Add item for preinstalled instance" << Log::Field("itemID", instanceStatuses.mItemID)
                      << Log::Field("type", instanceStatuses.mType) << Log::Field("version", instanceStatuses.mVersion);

            if (auto err = mUnitStatus.mUpdateItems->EmplaceBack(
                    UpdateItemStatus {instanceStatuses.mItemID, instanceStatuses.mType, instanceStatuses.mVersion, true,
                        ItemStateEnum::eInstalled, ErrorEnum::eNone});
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

// Delta unit status contains changed instances only, and the launcher can't be requested here as it notifies listeners
// under its lock: items are checked against all instances tracked by this handler.
Error UnitStatusHandler::SetItemsForFailedInstances()
{
    Error firstErr;

    mFailedItems.RemoveIf([this, &firstErr](const FailedItem& item) {
        const auto state = GetItemInstancesState(item.mItemID, item.mVersion);

        if (state.mNumInstances != 0 && state.mNumFailed == state.mNumInstances) {
            return false;
        }

        // Without instances the item state is defined by the image manager.
        if (state.mNumInstances != 0) {
            if (auto err = SetItemState(*state.mInstance, ItemStateEnum::eInstalled, ErrorEnum::eNone, true);
                !err.IsNone() && firstErr.IsNone()) {
                firstErr = err;
            }
        }

        return true;
    });

    for (const auto& instance : mItemInstances) {
        if (instance.mState != InstanceStateEnum::eFailed) {
            continue;
        }

        const auto state = GetItemInstancesState(instance.mIdent.mItemID, instance.mVersion);

        if (state.mNumFailed != state.mNumInstances) {
            continue;
        }

        const auto isNew = !mFailedItems.ContainsIf([&instance](const FailedItem& item) {
            return item.mItemID == instance.mIdent.mItemID && item.mVersion == instance.mVersion;
        });

        if (isNew) {
            if (auto err = mFailedItems.EmplaceBack(FailedItem {instance.mIdent.mItemID, instance.mVersion});
                !err.IsNone() && firstErr.IsNone()) {
                firstErr = AOS_ERROR_WRAP(err);
            }
        }

        // Delta unit status contains only changed items: the failed item is added once.
        if (auto err = SetItemState(instance, ItemStateEnum::eFailed, state.mError, isNew || !mUnitStatus.mIsDeltaInfo);
            !err.IsNone() && firstErr.IsNone()) {
            firstErr = err;
        }
    }

    return firstErr;
}

Error UnitStatusHandler::UpdateItemInstance(const InstanceStatus& status)
{
    const auto isSameInstance = [&status](const ItemInstance& instance) {
        return instance.mIdent == static_cast<const InstanceIdent&>(status) && instance.mVersion == status.mVersion;
    };

    if (status.mState == InstanceStateEnum::eInactive) {
        mItemInstances.RemoveIf(isSameInstance);

        return ErrorEnum::eNone;
    }

    if (auto it = mItemInstances.FindIf(isSameInstance); it != mItemInstances.end()) {
        it->mState = status.mState;
        it->mError = status.mError;

        return ErrorEnum::eNone;
    }

    if (auto err = mItemInstances.EmplaceBack(
            ItemInstance {static_cast<const InstanceIdent&>(status), status.mVersion, status.mState, status.mError});
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

// Only installed items are changed: other states are provided by the image manager, and its failure is not overridden.
Error UnitStatusHandler::SetItemState(const ItemInstance& instance, ItemState state, const Error& error, bool addItem)
{
    if (!mUnitStatus.mUpdateItems.HasValue()) {
        if (!addItem) {
            return ErrorEnum::eNone;
        }

        mUnitStatus.mUpdateItems.EmplaceValue();
    }

    auto it = mUnitStatus.mUpdateItems->FindIf([&instance](const UpdateItemStatus& itemStatus) {
        return itemStatus.mItemID == instance.mIdent.mItemID && itemStatus.mVersion == instance.mVersion;
    });
    if (it != mUnitStatus.mUpdateItems->end()) {
        if (it->mState == ItemStateEnum::eInstalled) {
            it->mState = state;
            it->mError = error;
        }

        return ErrorEnum::eNone;
    }

    if (!addItem) {
        return ErrorEnum::eNone;
    }

    if (auto err = mUnitStatus.mUpdateItems->EmplaceBack(UpdateItemStatus {instance.mIdent.mItemID,
            instance.mIdent.mType, instance.mVersion, instance.mIdent.mPreinstalled, state, error});
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

UnitStatusHandler::ItemInstancesState UnitStatusHandler::GetItemInstancesState(
    const String& itemID, const String& version) const
{
    ItemInstancesState state;

    for (const auto& instance : mItemInstances) {
        if (instance.mIdent.mItemID != itemID || instance.mVersion != version) {
            continue;
        }

        state.mInstance = &instance;
        state.mNumInstances++;

        if (instance.mState == InstanceStateEnum::eFailed) {
            state.mNumFailed++;

            if (state.mError.IsNone()) {
                state.mError = instance.mError;
            }
        }
    }

    return state;
}

} // namespace aos::cm::updatemanager
