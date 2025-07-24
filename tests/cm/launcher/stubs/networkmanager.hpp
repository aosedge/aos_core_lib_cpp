/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_NETWORKMANAGER_HPP_
#define AOS_CM_LAUNCHER_STUBS_NETWORKMANAGER_HPP_

#include <aos/cm/networkmanager.hpp>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace aos::cm::networkmanager {

/**
 * Stub implementation of NetworkManagerItf interface
 */
class NetworkManagerStub : public NetworkManagerItf {
public:
    /**
     * Initializes network manager stub.
     */
    void Init()
    {
        mCurrentIP = 0xAC110001;
        mSubnet    = "172.17.0.0/16";
    }

    /**
     * Prepares and assigns network parameters for a service instance.
     *
     * @param instanceIdent instance identifier.
     * @param networkID identifier of the target network.
     * @param nodeID node identifier.
     * @param instanceData input parameters for network setup.
     * @param[out] result result network parameters.
     * @return Error.
     */
    Error PrepareInstanceNetworkParameters(const InstanceIdent& instanceIdent, const String& networkID,
        const String& nodeID, const NetworkInstanceData& instanceData, NetworkParameters& result) override
    {
        (void)nodeID;
        (void)instanceData;

        mNetworkInfo[networkID.CStr()].insert(instanceIdent);

        mCurrentIP++;

        NetworkParameters params;

        params.mIP     = IPToString(mCurrentIP).c_str();
        params.mSubnet = mSubnet.c_str();
        params.mDNSServers.PushBack(cDNSServer);

        result = params;

        return ErrorEnum::eNone;
    }

    /**
     * Removes assigned network parameters for the specified service instance.
     *
     * @param instanceIdent instance identifier.
     * @param nodeID node identifier.
     * @return Error.
     */
    Error RemoveInstanceNetworkParameters(const InstanceIdent& instanceIdent, const String& nodeID) override
    {
        (void)nodeID;

        for (auto& [key, network] : mNetworkInfo) {
            network.erase(instanceIdent);
        }

        return ErrorEnum::eNone;
    }

    /**
     * Restarts DNS server.
     *
     * @return Error.
     */
    Error RestartDNSServer() override { return ErrorEnum::eNone; }

    /**
     * Returns all service instances registered in network manager.
     *
     * @param[out] instances list of instances.
     * @return Error.
     */
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

    /**
     * Updates network configuration for the given providers and node.
     *
     * @param providers a list of provider ids.
     * @param nodeID node identifier.
     * @return Error.
     */
    Error UpdateProviderNetwork(const Array<StaticString<cProviderIDLen>>& providers, const String& nodeID) override
    {
        (void)providers;
        (void)nodeID;

        return ErrorEnum::eNone;
    }

private:
    std::map<std::string, std::set<InstanceIdent>> mNetworkInfo;
    uint32_t                                       mCurrentIP;
    std::string                                    mSubnet;
    static constexpr auto                          cDNSServer = "10.10.0.1";

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
