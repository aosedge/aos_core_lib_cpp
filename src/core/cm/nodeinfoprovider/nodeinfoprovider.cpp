/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "nodeinfoprovider.hpp"

namespace aos::cm::nodeinfoprovider {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error NodeInfoProvider::Init(const Config& config, iamclient::NodeInfoProviderItf& nodeInfoProvider)
{
    LOG_DBG() << "Init node info provider";

    mNodeInfoProvider = &nodeInfoProvider;

    mConfig = config;

    return ErrorEnum::eNone;
}

Error NodeInfoProvider::Start()
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Start node info provider";

    if (mRunning) {
        return ErrorEnum::eWrongState;
    }

    auto ids = MakeUnique<StaticArray<StaticString<cIDLen>, cMaxNumNodes>>(&mAllocator);

    if (auto err = mNodeInfoProvider->GetAllNodeIDs(*ids); !err.IsNone()) {
        return err;
    }

    for (const auto& id : *ids) {
        auto nodeInfo = MakeUnique<NodeInfo>(&mAllocator);

        if (auto err = mNodeInfoProvider->GetNodeInfo(id, *nodeInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = mCache.EmplaceBack(mConfig.mSMConnectionTimeout, *nodeInfo); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = mNodeInfoProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mRunning = true;

    return mThread.Run([this](void*) { Run(); });
}

Error NodeInfoProvider::Stop()
{
    {
        LockGuard lock {mMutex};

        LOG_DBG() << "Stop node info provider";

        if (!mRunning) {
            return ErrorEnum::eWrongState;
        }

        mCache.Clear();

        if (auto err = mNodeInfoProvider->UnsubscribeListener(*this); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        mRunning = false;
        mCondVar.NotifyAll();
    }

    mThread.Join();

    return ErrorEnum::eNone;
}

Error NodeInfoProvider::GetAllNodeIDs(Array<StaticString<cIDLen>>& ids) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get all node ids";

    for (const auto& nodeInfo : mCache) {
        if (auto err = ids.EmplaceBack(nodeInfo.GetNodeID()); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error NodeInfoProvider::GetNodeInfo(const String& nodeID, UnitNodeInfo& nodeInfo) const
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get node info" << Log::Field("nodeID", nodeID);

    const auto it = mCache.FindIf([&nodeID](const auto& info) { return info.GetNodeID() == nodeID; });
    if (it == mCache.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    it->GetUnitNodeInfo(nodeInfo);

    return ErrorEnum::eNone;
}

Error NodeInfoProvider::SubscribeListener(nodeinfoprovider::NodeInfoListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe node info listener";

    if (const auto it = mListeners.Find(&listener); it != mListeners.end()) {
        return ErrorEnum::eAlreadyExist;
    }

    return mListeners.EmplaceBack(&listener);
}

Error NodeInfoProvider::UnsubscribeListener(nodeinfoprovider::NodeInfoListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unsubscribe node info listener";

    if (mListeners.Remove(&listener) == 0) {
        return ErrorEnum::eNotFound;
    }

    return ErrorEnum::eNone;
}

void NodeInfoProvider::OnSMConnected(const String& nodeID)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "SM connected" << Log::Field("nodeID", nodeID);

    auto it = mCache.FindIf([&nodeID](const auto& info) { return info.GetNodeID() == nodeID; });
    if (it == mCache.end()) {
        return;
    }

    it->OnSMConnected();

    if (auto err = ScheduleNotification(it->GetNodeID()); !err.IsNone()) {
        LOG_ERR() << "Failed to schedule notification" << Log::Field("nodeID", nodeID) << Log::Field(err);
    }
}

void NodeInfoProvider::OnSMDisconnected(const String& nodeID, const Error& err)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "SM disconnected" << Log::Field("nodeID", nodeID) << Log::Field(err);

    auto it = mCache.FindIf([&nodeID](const auto& info) { return info.GetNodeID() == nodeID; });
    if (it == mCache.end()) {
        return;
    }

    it->OnSMDisconnected();

    if (auto notifyErr = ScheduleNotification(it->GetNodeID()); !notifyErr.IsNone()) {
        LOG_ERR() << "Failed to schedule notification" << Log::Field("nodeID", nodeID) << Log::Field(notifyErr);
    }
}

Error NodeInfoProvider::OnSMInfoReceived(const SMInfo& info)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "SM info received" << Log::Field("nodeID", info.mNodeID);

    auto it = mCache.FindIf([&info](const auto& nodeInfo) { return nodeInfo.GetNodeID() == info.mNodeID; });
    if (it == mCache.end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = it->OnSMReceived(info); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ScheduleNotification(it->GetNodeID());
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void NodeInfoProvider::OnNodeInfoChanged(const NodeInfo& info)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Node info changed" << Log::Field("nodeID", info.mNodeID) << Log::Field("state", info.mState)
              << Log::Field("isConnected", info.mIsConnected) << Log::Field(info.mError);

    auto it = mCache.FindIf([&info](const auto& nodeInfo) { return nodeInfo.GetNodeID() == info.mNodeID; });
    if (it == mCache.end()) {
        if (auto err = mCache.EmplaceBack(mConfig.mSMConnectionTimeout, info); !err.IsNone()) {
            LOG_ERR() << "Failed to add node info to cache" << Log::Field(err);
            return;
        }

        it = &mCache.Back();
    } else {
        it->SetNodeInfo(info);
    }

    if (auto err = ScheduleNotification(it->GetNodeID()); !err.IsNone()) {
        LOG_ERR() << "Failed to schedule notification" << Log::Field("nodeID", it->GetNodeID()) << Log::Field(err);
    }
}

Error NodeInfoProvider::ScheduleNotification(const String& nodeID)
{
    if (mNotificationQueue.Find(nodeID) != mNotificationQueue.end()) {
        return ErrorEnum::eNone;
    }

    if (auto err = mNotificationQueue.EmplaceBack(nodeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mCondVar.NotifyAll();

    return ErrorEnum::eNone;
}

void NodeInfoProvider::Run()
{
    LOG_DBG() << "Running notification thread";

    while (true) {
        {
            UniqueLock lock {mMutex};

            mCondVar.Wait(lock, [this]() { return !mRunning || !mNotificationQueue.IsEmpty(); });

            if (!mRunning) {
                return;
            }

            for (const auto& nodeInfo : mCache) {
                if (!nodeInfo.IsReady()) {
                    LOG_DBG() << "Node info not ready" << Log::Field("nodeID", nodeInfo.GetNodeID());

                    continue;
                }

                auto it = mNotificationQueue.Find(nodeInfo.GetNodeID());
                if (it == mNotificationQueue.end()) {
                    LOG_DBG() << "No notification scheduled for node" << Log::Field("nodeID", nodeInfo.GetNodeID());

                    continue;
                }

                auto unitNodeInfo = MakeUnique<UnitNodeInfo>(&mAllocator);

                nodeInfo.GetUnitNodeInfo(*unitNodeInfo);

                LOG_DBG() << "Notifying listeners about node info change" << Log::Field("nodeID", unitNodeInfo->mNodeID)
                          << Log::Field("state", unitNodeInfo->mState)
                          << Log::Field("isConnected", unitNodeInfo->mIsConnected) << Log::Field(unitNodeInfo->mError);

                for (auto* listener : mListeners) {
                    listener->OnNodeInfoChanged(*unitNodeInfo);
                }

                mNotificationQueue.Erase(it);
            }

            mCondVar.Wait(lock, mConfig.mSMConnectionTimeout);
        }
    }
}

} // namespace aos::cm::nodeinfoprovider
