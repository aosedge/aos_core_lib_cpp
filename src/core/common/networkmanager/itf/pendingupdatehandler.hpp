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
    bool operator==(const PendingFirewallUpdate& rhs) const
    {
        return mInstanceIdent == rhs.mInstanceIdent && mFirewallRules == rhs.mFirewallRules;
    }

    /**
     * Compares pending firewall update.
     *
     * @param rhs pending firewall update to compare.
     * @return bool.
     */
    bool operator!=(const PendingFirewallUpdate& rhs) const { return !operator==(rhs); }
};

/**
 * Handler for pending firewall update notifications.
 * CM side: CM NetworkManager notifies SMController which pushes to stream.
 * SM side: SM NetworkManager implements this to apply resolved pending rules.
 */
class PendingUpdateHandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~PendingUpdateHandlerItf() = default;

    /**
     * Called when pending firewall rules are resolved for an instance.
     *
     * @param nodeID node ID where the instance resides.
     * @param update pending firewall update.
     */
    virtual void OnPendingFirewallUpdate(const String& nodeID, const PendingFirewallUpdate& update) = 0;
};

/** @}*/

} // namespace aos::networkmanager

#endif
