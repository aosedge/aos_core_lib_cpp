/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_FIREWALL_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_FIREWALL_HPP_

#include <core/common/tools/string.hpp>
#include <core/common/types/common.hpp>
#include <core/common/types/network.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Input access (exposed-port) rule.
 *
 * Translated to: `<proto> dport <port> accept` in the per-instance chain.
 */
struct InputAccessConfig {
    StaticString<cPortLen>         mPort;
    StaticString<cProtocolNameLen> mProtocol;
};

/**
 * Output access (egress allow) rule.
 *
 * Translated to: `ip daddr <dstIP> [ip saddr <srcIP>] <proto> dport <dstPort> accept`.
 */
struct OutputAccessConfig {
    StaticString<cSubnetLen>       mDstIP;
    StaticString<cPortLen>         mDstPort;
    StaticString<cProtocolNameLen> mProto;
    StaticString<cSubnetLen>       mSrcIP;
};

/**
 * Per-instance firewall parameters.
 */
struct InstanceFirewallParams {
    StaticString<cIPLen>                                  mIP;
    bool                                                  mAllowPublic {};
    StaticArray<InputAccessConfig, cMaxNumFirewallRules>  mInput;
    StaticArray<OutputAccessConfig, cMaxNumFirewallRules> mOutput;
};

/**
 * Firewall interface.
 *
 * Owns a single nft table (`inet aos`) for the whole SM. Each instance gets
 * its own chain installed into the shared base chain; updates run in a
 * single nft transaction (atomic delete-old + add-new).
 */
class FirewallItf {
public:
    /**
     * Destructor.
     */
    virtual ~FirewallItf() = default;

    /**
     * Starts the firewall: creates the `inet aos` table, base chains and
     * netfilter hooks (forward filter, postrouting nat).
     *
     * @return Error.
     */
    virtual Error Start() = 0;

    /**
     * Stops the firewall: deletes the `inet aos` table.
     *
     * @return Error.
     */
    virtual Error Stop() = 0;

    /**
     * Adds a per-instance chain with the given input/output access rules.
     *
     * @param instanceID instance id.
     * @param params per-instance firewall parameters.
     * @return Error.
     */
    virtual Error AddInstance(const String& instanceID, const InstanceFirewallParams& params) = 0;

    /**
     * Removes the per-instance chain.
     *
     * @param instanceID instance id.
     * @return Error.
     */
    virtual Error RemoveInstance(const String& instanceID) = 0;

    /**
     * Atomically replaces the per-instance chain content with new rules.
     *
     * Backing the OnPendingFirewallUpdate path; performed as a single nft
     * transaction so there is no window where rules are missing.
     *
     * @param instanceID instance id.
     * @param params new per-instance firewall parameters.
     * @return Error.
     */
    virtual Error UpdateInstance(const String& instanceID, const InstanceFirewallParams& params) = 0;

    /**
     * Adds an IPMasq rule for the given subnet (egress via any interface but
     * outIf, i.e. the bridge is excluded).
     *
     * Installed once per network on creation. Idempotent for repeated
     * subnet/outIf pairs.
     *
     * @param subnet source subnet (CIDR).
     * @param outIf bridge interface name to exclude from masquerading.
     * @return Error.
     */
    virtual Error AddMasquerade(const String& subnet, const String& outIf) = 0;

    /**
     * Removes the IPMasq rule for the given subnet/outIf pair.
     *
     * Removed once per network on teardown.
     *
     * @param subnet source subnet (CIDR).
     * @param outIf output interface name.
     * @return Error.
     */
    virtual Error RemoveMasquerade(const String& subnet, const String& outIf) = 0;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
