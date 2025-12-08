/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "monitoring.hpp"

namespace aos::cm::monitoring {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Monitoring::Init(const Config& config, SenderItf& sender,
    instancestatusprovider::ProviderItf&   instanceStatusProvider,
    nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider)
{
    LOG_DBG() << "Initialize monitoring" << Log::Field("sendPeriod", config.mSendPeriod);

    mConfig                 = config;
    mSender                 = &sender;
    mInstanceStatusProvider = &instanceStatusProvider;
    mNodeInfoProvider       = &nodeInfoProvider;

    return ErrorEnum::eNone;
}

Error Monitoring::Start()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start monitoring module";

    if (mIsRunning) {
        return ErrorEnum::eWrongState;
    }

    Error err;

    auto unsubscribeOnError = DeferRelease(&err, [this](const Error* err) {
        if (!err->IsNone()) {
            mInstanceStatusProvider->UnsubscribeListener(*this);
            mNodeInfoProvider->UnsubscribeListener(*this);
        }
    });

    err = mInstanceStatusProvider->SubscribeListener(*this);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mNodeInfoProvider->SubscribeListener(*this);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mIsRunning = true;

    err = mSendTimer.Start(
        mConfig.mSendPeriod,
        [this](void*) {
            if (auto sendErr = SendMonitoringData(); !sendErr.IsNone()) {
                LOG_ERR() << "Failed to send monitoring" << Log::Field(sendErr);
            }
        },
        false);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Monitoring::Stop()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Stop monitoring module";

    if (!mIsRunning) {
        return ErrorEnum::eWrongState;
    }

    mInstanceStatusProvider->UnsubscribeListener(*this);
    mNodeInfoProvider->UnsubscribeListener(*this);

    mIsRunning = false;

    return mSendTimer.Stop();
}

Error Monitoring::OnMonitoringReceived(const aos::monitoring::NodeMonitoringData& monitoring)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Monitoring data received" << Log::Field("nodeID", monitoring.mNodeID);

    return CacheMonitoringData(monitoring);
}

void Monitoring::OnNodeInfoChanged(const UnitNodeInfo& info)
{
    LockGuard lock {mMutex};

    NodeStateInfo stateInfo {
        Time::Now(),
        info.mState,
        info.mIsConnected,
    };

    LOG_DBG() << "Node info changed" << Log::Field("nodeID", info.mNodeID);

    auto it
        = mMonitoring.mNodes.FindIf([&info](const NodeMonitoringData& data) { return data.mNodeID == info.mNodeID; });
    if (it == mMonitoring.mNodes.end()) {
        if (auto err = mMonitoring.mNodes.EmplaceBack(); !err.IsNone()) {
            LOG_ERR() << "Failed to add node monitoring data" << Log::Field(err);
            return;
        }

        it = &mMonitoring.mNodes.Back();

        it->mNodeID = info.mNodeID;
    }

    if (it->mStates.IsEmpty()) {
        it->mStates.PushBack(stateInfo);

        return;
    }

    if (it->mStates.Back().mState == stateInfo.mState && it->mStates.Back().mIsConnected == stateInfo.mIsConnected) {
        return;
    }

    if (it->mStates.IsFull()) {
        it->mStates.Erase(it->mStates.begin());
    }

    it->mStates.PushBack(stateInfo);
}

void Monitoring::OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Instances statuses changed" << Log::Field("count", statuses.Size());

    const auto now = Time::Now();

    for (const auto& status : statuses) {
        auto it = mMonitoring.mInstances.FindIf([&status](const InstanceMonitoringData& data) {
            return static_cast<const InstanceIdent&>(data) == static_cast<const InstanceIdent&>(status)
                && data.mNodeID == status.mNodeID;
        });

        if (it == mMonitoring.mInstances.end()) {
            if (auto err = mMonitoring.mInstances.EmplaceBack(); !err.IsNone()) {
                LOG_ERR() << "Failed to add instance monitoring data" << Log::Field(AOS_ERROR_WRAP(err));
                continue;
            }

            it = &mMonitoring.mInstances.Back();

            static_cast<InstanceIdent&>(*it) = static_cast<const InstanceIdent&>(status);
            it->mNodeID                      = status.mNodeID;
        }

        if (it->mStates.IsFull()) {
            it->mStates.Erase(it->mStates.begin());
        }

        it->mStates.PushBack({now, status.mState});
    }
}

void Monitoring::OnConnect()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Publisher connected";

    mIsConnected = true;
}

void Monitoring::OnDisconnect()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Publisher disconnected";

    mIsConnected = false;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Monitoring::FillNodeMonitoring(const String& nodeID, const aos::monitoring::NodeMonitoringData& nodeMonitoring)
{
    auto it = mMonitoring.mNodes.FindIf([&nodeID](const NodeMonitoringData& data) { return data.mNodeID == nodeID; });
    if (it == mMonitoring.mNodes.end()) {
        if (auto err = mMonitoring.mNodes.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        it = &mMonitoring.mNodes.Back();

        it->mNodeID = nodeID;
    }

    if (it->mItems.IsFull()) {
        it->mItems.Erase(it->mItems.begin());
    }

    return it->mItems.EmplaceBack(nodeMonitoring.mMonitoringData);
}

Error Monitoring::FillInstanceMonitoring(const aos::monitoring::InstanceMonitoringData& instanceMonitoring)
{
    auto it = mMonitoring.mInstances.FindIf([&instanceMonitoring](const InstanceMonitoringData& data) {
        return static_cast<const InstanceIdent&>(data) == instanceMonitoring.mInstanceIdent;
    });

    if (it == mMonitoring.mInstances.end()) {
        if (auto err = mMonitoring.mInstances.EmplaceBack(); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        it = &mMonitoring.mInstances.Back();

        static_cast<InstanceIdent&>(*it) = instanceMonitoring.mInstanceIdent;
    }

    if (it->mItems.IsFull()) {
        it->mItems.Erase(it->mItems.begin());
    }

    return it->mItems.EmplaceBack(instanceMonitoring.mMonitoringData);
}

Error Monitoring::CacheMonitoringData(const aos::monitoring::NodeMonitoringData& monitoringData)
{
    if (auto err = FillNodeMonitoring(monitoringData.mNodeID, monitoringData); !err.IsNone()) {
        return err;
    }

    for (const auto& instanceMonitoring : monitoringData.mInstances) {
        if (auto err = FillInstanceMonitoring(instanceMonitoring); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error Monitoring::SendMonitoringData()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Process monitoring";

    if (!mIsRunning || !mIsConnected || (mMonitoring.mNodes.IsEmpty() && mMonitoring.mInstances.IsEmpty())) {
        return ErrorEnum::eNone;
    }

    if (auto err = mSender->SendMonitoring(mMonitoring); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mMonitoring.mNodes.Clear();
    mMonitoring.mInstances.Clear();

    return ErrorEnum::eNone;
}

} // namespace aos::cm::monitoring
