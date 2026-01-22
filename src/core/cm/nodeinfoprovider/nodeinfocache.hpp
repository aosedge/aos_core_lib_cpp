/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_NODEINFOPROVIDER_NODEINFOCACHE_HPP_
#define AOS_CORE_CM_NODEINFOPROVIDER_NODEINFOCACHE_HPP_

#include <core/common/tools/time.hpp>
#include <core/common/types/unitstatus.hpp>

#include "itf/sminforeceiver.hpp"

namespace aos::cm::nodeinfoprovider {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Node info cache.
 */
class NodeInfoCache {
public:
    /**
     * Constructor.
     *
     * @param waitTimeout wait timeout.
     * @param info node info.
     */
    NodeInfoCache(const Duration waitTimeout, const NodeInfo& info);

    /**
     * Constructor.
     *
     * @param waitTimeout wait timeout.
     * @param nodeID node ID.
     */
    NodeInfoCache(const Duration waitTimeout, const String& nodeID);

    /**
     * Returns node ID.
     *
     * @return const String&.
     */
    const String& GetNodeID() const { return mNodeID; }

    /**
     * Sets node info.
     *
     * @param info node info.
     */
    void SetNodeInfo(const NodeInfo& info);

    /**
     * Returns unit node info.
     *
     * @param info node info.
     */
    void GetUnitNodeInfo(UnitNodeInfo& info) const;

    /**
     * Notifies that SM is connected.
     */
    void OnSMConnected();

    /**
     * Notifies that SM is disconnected.
     */
    void OnSMDisconnected();

    /**
     * Processes received SM info.
     *
     * @param info SM info.
     * @return Error.
     */
    Error OnSMReceived(const SMInfo& info);

    /**
     *  Returns is connected flag.
     *
     * @return bool.
     */
    bool IsConnected() const;

    /**
     * Checks whether node info is ready.
     *
     * @return bool.
     */
    bool IsReady() const;

private:
    void SetNodeInfo(UnitNodeInfo& info) const;
    void SetSMInfo(UnitNodeInfo& info) const;

    Duration             mWaitTimeout;
    StaticString<cIDLen> mNodeID;
    Optional<NodeInfo>   mNodeInfo;
    Optional<SMInfo>     mSMInfo;
    Time                 mLastUpdate {Time::Now()};
    bool                 mSMReceived {};
    bool                 mHasSMComponent {true};
};

/** @}*/

} // namespace aos::cm::nodeinfoprovider

#endif
