/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_INTERFACEMANAGER_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_INTERFACEMANAGER_HPP_

#include <core/common/types/common.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Network interface manager interface.
 */
class InterfaceManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~InterfaceManagerItf() = default;

    /**
     * Removes interface.
     *
     * @param ifname interface name.
     * @return Error.
     */
    virtual Error DeleteLink(const String& ifname) = 0;

    /**
     * Sets up link (brings it up).
     *
     * If netNSPath is non-empty, the operation runs inside that namespace.
     * Required for interfaces moved into a netns: the kernel administratively
     * downs a link on a namespace move, so IFF_UP must be re-applied there.
     *
     * @param ifname interface name.
     * @param netNSPath optional path to the netns; empty for current.
     * @return Error.
     */
    virtual Error SetupLink(const String& ifname, const String& netNSPath = "") = 0;

    /**
     * Sets master.
     *
     * @param ifname interface name.
     * @param master master interface name.
     * @return Error.
     */
    virtual Error SetMasterLink(const String& ifname, const String& master) = 0;

    /**
     * Creates a veth pair. Both ends are initially in the current netns.
     *
     * @param hostIfName host-side veth name.
     * @param peerIfName peer-side veth name.
     * @return Error.
     */
    virtual Error CreateVeth(const String& hostIfName, const String& peerIfName) = 0;

    /**
     * Creates a veth pair with the peer placed directly into the given network
     * namespace, already named as peerIfName. The host side stays in the
     * current namespace. Combines veth creation, namespace move and rename into
     * a single netlink operation.
     *
     * @param hostIfName host-side veth name (current namespace).
     * @param peerIfName peer-side veth name inside the target namespace.
     * @param netNSPath path to the target netns (e.g. /run/netns/<id>).
     * @param master master bridge name to enslave the host side to in the same
     *               operation; empty for none.
     * @return Error.
     */
    virtual Error CreateVethToNamespace(
        const String& hostIfName, const String& peerIfName, const String& netNSPath, const String& master)
        = 0;

    /**
     * Configures an interface inside a network namespace in a single namespace
     * entry: brings it up, assigns the address and installs the default route.
     *
     * @param ifname interface name inside the namespace.
     * @param ipWithMask IP in CIDR form, e.g. "10.0.0.5/24".
     * @param gateway default-route gateway IP.
     * @param netNSPath path to the instance netns.
     * @return Error.
     */
    virtual Error ConfigureInstanceInterface(
        const String& ifname, const String& ipWithMask, const String& gateway, const String& netNSPath)
        = 0;

    /**
     * Moves a link into a network namespace identified by its /run/netns path.
     *
     * @param ifname interface name.
     * @param netNSPath path to the target netns (e.g. /run/netns/<id>).
     * @return Error.
     */
    virtual Error MoveLinkToNamespace(const String& ifname, const String& netNSPath) = 0;

    /**
     * Renames a link.
     *
     * The link must be down. If netNSPath is non-empty, the operation runs
     * inside that namespace.
     *
     * @param ifname current interface name.
     * @param newName new interface name.
     * @param netNSPath optional path to the netns; empty for current.
     * @return Error.
     */
    virtual Error RenameLink(const String& ifname, const String& newName, const String& netNSPath) = 0;

    /**
     * Assigns an IP address (CIDR form "ip/mask") to an interface.
     *
     * If netNSPath is non-empty, the operation runs inside that namespace.
     *
     * @param ifname interface name.
     * @param ipWithMask IP in CIDR form, e.g. "10.0.0.5/24".
     * @param netNSPath optional path to the netns; empty for current.
     * @return Error.
     */
    virtual Error AddAddress(const String& ifname, const String& ipWithMask, const String& netNSPath) = 0;

    /**
     * Adds a route. destination may be "0.0.0.0/0" for default.
     *
     * If netNSPath is non-empty, the operation runs inside that namespace.
     *
     * @param destination destination prefix in CIDR form.
     * @param gateway gateway IP.
     * @param netNSPath optional path to the netns; empty for current.
     * @return Error.
     */
    virtual Error AddRoute(const String& destination, const String& gateway, const String& netNSPath) = 0;

    /**
     * Enables/disables hairpin mode on a bridge port (the host-side veth
     * attached to a bridge).
     *
     * @param ifname interface name (host-side veth).
     * @param enable enable/disable.
     * @return Error.
     */
    virtual Error SetHairpin(const String& ifname, bool enable) = 0;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
