/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_NETWORKMANAGERSTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_NETWORKMANAGERSTUB_HPP_

#include <map>
#include <set>
#include <sstream>
#include <string>

#include <core/cm/networkmanager/itf/networkmanager.hpp>

namespace aos::cm::networkmanager {

class NetworkManagerStub : public NetworkManagerItf {
public:
    void Init()
    {
        mNetworkInfo.clear();
        mCurrentIP = 0xAC110001;
    }

    Error PrepareInstanceNetworkParameters(const InstanceIdent& instanceIdent, const String& networkID,
        const String& nodeID, const NetworkServiceData& networkData, InstanceNetworkParameters& result) override
    {
        (void)nodeID;
        (void)networkData;

        mNetworkInfo[networkID.CStr()].insert(instanceIdent);

        mCurrentIP++;

        InstanceNetworkParameters params;

        params.mIP     = IPToString(mCurrentIP).c_str();
        params.mSubnet = mSubnet.c_str();

        result = params;

        return ErrorEnum::eNone;
    }

    Error RemoveInstanceNetworkParameters(const InstanceIdent& instanceIdent, const String& nodeID) override
    {
        (void)nodeID;

        for (auto& [key, network] : mNetworkInfo) {
            network.erase(instanceIdent);
        }

        return ErrorEnum::eNone;
    }

    Error RestartDNSServer() override { return ErrorEnum::eNone; }

    Error GetInstances(Array<InstanceIdent>& instances) const override
    {
        instances.Clear();

        for (const auto& [key, network] : mNetworkInfo) {
            for (const auto& instanceIdent : network) {
                if (auto err = instances.PushBack(instanceIdent); !err.IsNone()) {
                    return err;
                }
            }
        }

        return ErrorEnum::eNone;
    }

    Error UpdateProviderNetwork(const Array<StaticString<cIDLen>>& providers, const String& nodeID) override
    {
        (void)providers;
        (void)nodeID;

        return ErrorEnum::eNone;
    }

private:
    std::map<std::string, std::set<InstanceIdent>> mNetworkInfo;
    uint32_t                                       mCurrentIP {0xAC110001};
    std::string                                    mSubnet {"172.17.0.0/16"};

    std::string IPToString(uint32_t ip) const
    {
        uint8_t a = (ip >> 24) & 0xFF;
        uint8_t b = (ip >> 16) & 0xFF;
        uint8_t c = (ip >> 8) & 0xFF;
        uint8_t d = ip & 0xFF;

        std::ostringstream oss;
        oss << static_cast<int>(a) << '.' << static_cast<int>(b) << '.' << static_cast<int>(c) << '.'
            << static_cast<int>(d);

        return oss.str();
    }
};

} // namespace aos::cm::networkmanager

#endif
