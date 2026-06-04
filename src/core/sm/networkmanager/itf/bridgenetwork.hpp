/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_BRIDGENETWORK_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_BRIDGENETWORK_HPP_

#include <core/common/consts.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/types/common.hpp>
#include <core/common/types/network.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Default container-side veth interface name.
 */
constexpr auto cDefaultContainerIfName = "eth0";

/**
 * Bridge parameters.
 *
 * Describes a per-instance bridge attach: a host/peer veth pair where the
 * host end is attached to mBridgeIfName, the peer end is moved into
 * mNetNSPath, and mIPWithMask + a default route via mGateway are assigned.
 */
struct BridgeParams {
    StaticString<cInterfaceLen> mBridgeIfName;
    StaticString<cFilePathLen>  mNetNSPath;
    StaticString<cInterfaceLen> mContainerIfName;
    StaticString<cSubnetLen>    mIPWithMask;
    StaticString<cIPLen>        mGateway;
    bool                        mHairpin {};
};

/**
 * Bridge attach result.
 */
struct BridgeAttachResult {
    StaticString<cInterfaceLen> mHostIfName;
    StaticString<cInterfaceLen> mContainerIfName;
};

/**
 * Bridge network interface.
 *
 * Owns the per-instance veth/addr/route/hairpin lifecycle.
 */
class BridgeNetworkItf {
public:
    /**
     * Destructor.
     */
    virtual ~BridgeNetworkItf() = default;

    /**
     * Attaches an instance to the bridge.
     *
     * Creates a veth pair, attaches the host end to the bridge, moves the
     * peer end into the instance netns, assigns IP/route and sets hairpin.
     *
     * @param instanceID instance id.
     * @param params bridge parameters.
     * @param[out] result attach result (host/container interface names).
     * @return Error.
     */
    virtual Error Attach(const String& instanceID, const BridgeParams& params, BridgeAttachResult& result) = 0;

    /**
     * Detaches an instance from the bridge.
     *
     * Removes the host-side veth (which auto-removes the peer).
     *
     * @param instanceID instance id.
     * @param bridgeIfName bridge interface name.
     * @return Error.
     */
    virtual Error Detach(const String& instanceID, const String& bridgeIfName) = 0;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
