/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_DNSNAME_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_DNSNAME_HPP_

#include <core/common/tools/error.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/types/common.hpp>
#include <core/common/types/network.hpp>

#include "types.hpp"

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Per-instance DNS aliases. Network/domain is scoped by the owning
 * DNSServerItf (one per network), so it is not repeated here.
 */
struct DNSAliasesParams {
    StaticString<cIPLen>                                  mIP;
    StaticArray<StaticString<cHostNameLen>, cMaxNumHosts> mAliases;
};

/**
 * Per-network DNS server configuration (one dnsmasq per bridge).
 */
struct DNSServerParams {
    /**
     * dnsmasq listen address (the bridge IP).
     */
    StaticString<cIPLen> mBridgeIP;
    /**
     * Bridge interface name dnsmasq binds to.
     */
    StaticString<cInterfaceLen> mBridgeIfName;
};

/**
 * DNS server — handle to the DNS backend of one network/bridge. Created
 * and owned by DNSNameItf; the caller does not delete it.
 */
class DNSServerItf {
public:
    /**
     * Destructor.
     */
    virtual ~DNSServerItf() = default;

    /**
     * Adds a host (instance) and its aliases.
     *
     * @param instanceID instance id.
     * @param params per-instance DNS parameters.
     * @return Error.
     */
    virtual Error AddHost(const String& instanceID, const DNSAliasesParams& params) = 0;

    /**
     * Removes a host (instance).
     *
     * @param instanceID instance id.
     * @return Error.
     */
    virtual Error RemoveHost(const String& instanceID) = 0;
};

/**
 * DNS name interface — factory of per-network DNS servers. No global
 * Start/Stop: the DNS lifecycle is per network only, and the backend owns
 * the dnsmasq process itself (not externally provisioned).
 */
class DNSNameItf {
public:
    /**
     * Destructor.
     */
    virtual ~DNSNameItf() = default;

    /**
     * Removes orphan DNS servers left over from a previous SM lifetime.
     * Tears down any dnsmasq state whose networkID is not in
     * @p knownNetworkIDs — networks that were released while SM was down.
     * Called once from NetworkManager::Start, before any CreateServer.
     *
     * @param knownNetworkIDs networkIDs still present in storage.
     * @return Error.
     */
    virtual Error RemoveOrphans(const Array<StaticString<cIDLen>>& knownNetworkIDs) = 0;

    /**
     * Creates (or adopts) the DNS server for a network. Idempotent:
     * repeated calls for the same networkID return the same DNSServerItf*.
     * A dnsmasq still alive for this networkID from a previous SM lifetime
     * must be adopted or killed-and-respawned here.
     *
     * @param networkID network id; also acts as the DNS domain.
     * @param params per-network DNS configuration.
     * @return RetWithError<DNSServerItf*> handle owned by the factory.
     */
    virtual RetWithError<DNSServerItf*> CreateServer(const String& networkID, const DNSServerParams& params) = 0;

    /**
     * Removes the DNS server for a network.
     *
     * @param networkID network id.
     * @return Error.
     */
    virtual Error RemoveServer(const String& networkID) = 0;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
