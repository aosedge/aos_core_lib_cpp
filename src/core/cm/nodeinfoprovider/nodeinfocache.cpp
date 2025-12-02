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
    , mHasSMComponent(info.ContainsComponent(CoreComponentEnum::eSM))
{
    static_cast<NodeInfo&>(mNodeInfo) = info;
}

void NodeInfoCache::SetNodeInfo(const NodeInfo& info)
{
    mHasSMComponent                   = info.ContainsComponent(CoreComponentEnum::eSM);
    static_cast<NodeInfo&>(mNodeInfo) = info;
}

void NodeInfoCache::GetUnitNodeInfo(UnitNodeInfo& info) const
{
    info = mNodeInfo;

    if (!mHasSMComponent || mSMReceived) {
        return;
    }

    if (info.mState == NodeStateEnum::eError || !info.mIsConnected) {
        return;
    }

    if (Time::Now().Sub(mLastUpdate) > mWaitTimeout) {
        info.mState = NodeStateEnum::eError;
        info.mError = Error(ErrorEnum::eTimeout, "SM connection timeout");
    } else {
        info.mIsConnected = false;
    }
}

void NodeInfoCache::OnSMConnected()
{
    mSMReceived = false;
    mLastUpdate = Time::Now();
}

void NodeInfoCache::OnSMDisconnected()
{
    mSMReceived = false;
    mLastUpdate = Time::Now();
}

Error NodeInfoCache::OnSMReceived(const SMInfo& info)
{
    if (auto err = mNodeInfo.mResources.Assign(info.mResources); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNodeInfo.mRuntimes.Assign(info.mRuntimes); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mSMReceived = true;
    mLastUpdate = Time::Now();

    return ErrorEnum::eNone;
}

bool NodeInfoCache::IsReady() const
{
    if (!mHasSMComponent || mSMReceived) {
        return true;
    }

    return Time::Now().Sub(mLastUpdate) > mWaitTimeout;
}

} // namespace aos::cm::nodeinfoprovider
