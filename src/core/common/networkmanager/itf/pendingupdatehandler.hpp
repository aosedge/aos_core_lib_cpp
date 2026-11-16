/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_NETWORKMANAGER_ITF_PENDINGUPDATEHANDLER_HPP_
#define AOS_CORE_COMMON_NETWORKMANAGER_ITF_PENDINGUPDATEHANDLER_HPP_

#include <core/common/types/network.hpp>

namespace aos::networkmanager {

/** @addtogroup common Common
 *  @{
 */

/**
 * Pending firewall update.
 *
 * Carries the instance's whole current rule set, resolved from all of its allowed
 * connections against the current target addresses. A receiver has to replace the
 * rules it holds for the instance with these, so rules to released or re-addressed
 * targets are removed.
 */
struct PendingFirewallUpdate {
    InstanceIdent                                   mInstanceIdent;
    StaticArray<FirewallRule, cMaxNumFirewallRules> mFirewallRules;

    /**
     * Compares pending firewall update.
     *
     * @param rhs pending firewall update to compare.
     * @return bool.
     */
    friend bool operator==(const PendingFirewallUpdate& lhs, const PendingFirewallUpdate& rhs)
    {
        return lhs.mInstanceIdent == rhs.mInstanceIdent && lhs.mFirewallRules == rhs.mFirewallRules;
    };

    /**
     * Compares pending firewall update.
     *
     * @param rhs pending firewall update to compare.
     * @return bool.
     */
    friend bool operator!=(const PendingFirewallUpdate& lhs, const PendingFirewallUpdate& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * Handler for CM -> SM network notifications: pending firewall updates and unit networks updates.
 * CM side: CM NetworkManager notifies SMController which pushes to stream.
 * SM side: SM NetworkManager implements this to apply resolved pending rules and to keep the node networks in sync
 * with the networks of the unit.
 */
class PendingUpdateHandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~PendingUpdateHandlerItf() = default;

    /**
     * Called when the firewall rules of an instance change.
     *
     * @param nodeID node ID where the instance resides.
     * @param update pending firewall update.
     */
    virtual void OnPendingFirewallUpdate(const String& nodeID, const PendingFirewallUpdate& update) = 0;

    /**
     * Called with the full set of network IDs that exist in the unit.
     *
     * The set is complete, not a delta: a receiver has to replace the unit networks it holds with this set. Sent when
     * SM subscribes and whenever the set changes.
     *
     * @param nodeID node ID the update is addressed to.
     * @param networkIDs IDs of all networks of the unit.
     */
    virtual void OnUnitNetworksUpdate(const String& nodeID, const Array<StaticString<cIDLen>>& networkIDs) = 0;
};

/** @}*/

} // namespace aos::networkmanager

#endif
