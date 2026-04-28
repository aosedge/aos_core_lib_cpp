/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_DNSNAME_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_DNSNAME_HPP_

#include <core/common/tools/string.hpp>
#include <core/common/types/common.hpp>
#include <core/common/types/network.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Per-instance DNS aliases.
 *
 * mNetworkID acts as the DNS domain (so aliases resolve as
 * "<alias>.<networkID>" and the bare hostname inside that domain).
 */
struct DNSAliasesParams {
    StaticString<cHostNameLen>                            mNetworkID;
    StaticString<cIPLen>                                  mIP;
    StaticArray<StaticString<cHostNameLen>, cMaxNumHosts> mAliases;
};

/**
 * DNS name interface.
 *
 * Owns the DNS-name backend (default: local dnsmasq, addnhosts + SIGHUP via
 * pidfile — same pattern as cm::DNSServer).
 */
class DNSNameItf {
public:
    /**
     * Destructor.
     */
    virtual ~DNSNameItf() = default;

    /**
     * Starts the DNS-name backend (ensures dnsmasq is running, captures pid).
     *
     * @return Error.
     */
    virtual Error Start() = 0;

    /**
     * Stops the DNS-name backend.
     *
     * @return Error.
     */
    virtual Error Stop() = 0;

    /**
     * Registers an instance and its aliases.
     *
     * @param instanceID instance id.
     * @param params per-instance DNS parameters.
     * @return Error.
     */
    virtual Error AddInstance(const String& instanceID, const DNSAliasesParams& params) = 0;

    /**
     * Removes the instance's DNS entries.
     *
     * @param instanceID instance id.
     * @return Error.
     */
    virtual Error RemoveInstance(const String& instanceID) = 0;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
