/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "nodeinfocache.hpp"

namespace aos::cm::nodeinfoprovider {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

NodeInfoCache::NodeInfoCache(const Duration waitTimeout, const NodeInfo& info)
    : mWaitTimeout(waitTimeout)
    , mNodeID(info.mNodeID)
    , mNodeInfo(info)
    , mHasSMComponent(info.ContainsComponent(CoreComponentEnum::eSM))
{
}

NodeInfoCache::NodeInfoCache(const Duration waitTimeout, const String& nodeID)
    : mWaitTimeout(waitTimeout)
    , mNodeID(nodeID)
{
}

void NodeInfoCache::SetNodeInfo(const NodeInfo& info)
{
    mHasSMComponent = info.ContainsComponent(CoreComponentEnum::eSM);
    mNodeInfo.SetValue(info);
}

void NodeInfoCache::GetUnitNodeInfo(UnitNodeInfo& info) const
{
    SetNodeInfo(info);
    SetSMInfo(info);

    info.mIsConnected = IsConnected();

    if (info.mIsConnected || info.mState == NodeStateEnum::eError) {
        return;
    }

    if (Time::Now().Sub(mLastUpdate) > mWaitTimeout) {
        info.mState = NodeStateEnum::eError;
        info.mError = Error(ErrorEnum::eTimeout, "SM connection timeout");
    }
}

void NodeInfoCache::OnSMConnected()
{
    mSMInfo.Reset();
    mLastUpdate = Time::Now();
}

void NodeInfoCache::OnSMDisconnected()
{
    mSMInfo.Reset();
    mLastUpdate = Time::Now();
}

Error NodeInfoCache::OnSMReceived(const SMInfo& info)
{
    mSMInfo.SetValue(info);
    mLastUpdate = Time::Now();

    return ErrorEnum::eNone;
}

bool NodeInfoCache::IsConnected() const
{
    if (!mNodeInfo.HasValue()) {
        LOG_DBG() << "Node info not available yet" << Log::Field("nodeID", mNodeID);

        return false;
    }

    if (!mHasSMComponent) {
        return true;
    }

    if (!mSMInfo.HasValue()) {
        LOG_DBG() << "SM info not available yet" << Log::Field("nodeID", mNodeID);

        return false;
    }

    return true;
}

bool NodeInfoCache::IsReady() const
{
    return IsConnected() || Time::Now().Sub(mLastUpdate) > mWaitTimeout;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void NodeInfoCache::SetNodeInfo(UnitNodeInfo& info) const
{
    if (mNodeInfo.HasValue()) {
        static_cast<NodeInfo&>(info) = *mNodeInfo;

        return;
    }

    info.mNodeID = mNodeID;
}

void NodeInfoCache::SetSMInfo(UnitNodeInfo& info) const
{
    if (!mSMInfo.HasValue()) {
        return;
    }

    info.mResources = mSMInfo->mResources;
    info.mRuntimes  = mSMInfo->mRuntimes;
}

} // namespace aos::cm::nodeinfoprovider
