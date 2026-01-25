/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "networkmanager.hpp"

namespace aos::cm::launcher {

void NetworkManager::PrepareForBalancing()
{
    mNetworkServiceData.Clear();
}

Error NetworkManager::SetNetworkServiceData(
    const InstanceIdent& instanceIdent, const networkmanager::NetworkServiceData& data)
{
    if (auto err = mNetworkServiceData.Set(instanceIdent, data); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NetworkManager::PrepareInstanceNetworkParameters(const InstanceIdent& instanceIdent, const String& networkID,
    const String& nodeID, bool onlyExposedPorts, Optional<InstanceNetworkParameters>& result)
{
    const auto it = mNetworkServiceData.Find(instanceIdent);
    if (it == mNetworkServiceData.end()) {
        // Network is not configured for this instance
        return ErrorEnum::eNone;
    }

    if (onlyExposedPorts && it->mSecond.mExposedPorts.IsEmpty()) {
        // Skip as no exposed ports are configured for this instance
        return ErrorEnum::eNone;
    }

    result.EmplaceValue();

    return mNetMgr->PrepareInstanceNetworkParameters(instanceIdent, networkID, nodeID, it->mSecond, result.GetValue());
}

Error NetworkManager::RemoveInstanceNetworkParameters(const InstanceIdent& instanceIdent, const String& nodeID)
{
    return mNetMgr->RemoveInstanceNetworkParameters(instanceIdent, nodeID);
}

Error NetworkManager::RestartDNSServer()
{
    return mNetMgr->RestartDNSServer();
}

Error NetworkManager::UpdateProviderNetwork(const Array<StaticString<cIDLen>>& providers, const String& nodeID)
{
    return mNetMgr->UpdateProviderNetwork(providers, nodeID);
}

} // namespace aos::cm::launcher
