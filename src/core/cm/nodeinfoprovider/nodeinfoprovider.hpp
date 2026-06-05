/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_NODEINFOPROVIDER_NODEINFOPROVIDER_HPP_
#define AOS_CORE_CM_NODEINFOPROVIDER_NODEINFOPROVIDER_HPP_

#include <core/common/tools/allocator.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/timer.hpp>

#include <core/common/iamclient/itf/nodeinfoprovider.hpp>

#include "config.hpp"
#include "itf/nodeinfoprovider.hpp"
#include "itf/sminforeceiver.hpp"
#include "nodeinfocache.hpp"

namespace aos::cm::nodeinfoprovider {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Node info provider.
 */
class NodeInfoProvider : public NodeInfoProviderItf, public SMInfoReceiverItf, private iamclient::NodeInfoListenerItf {
public:
    /**
     * Initializes node info provider.
     *
     * @param config configuration.
     * @param nodeInfoProvider IAM client node info provider.
     * @return Error.
     */
    Error Init(const Config& config, iamclient::NodeInfoProviderItf& nodeInfoProvider);

    /**
     * Starts node info provider.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops node info provider.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Returns ids for all the nodes of the unit.
     *
     * @param[out] ids result node identifiers.
     * @return Error.
     */
    Error GetAllNodeIDs(Array<StaticString<cIDLen>>& ids) const override;

    /**
     * Returns info for specified node.
     *
     * @param nodeID node identifier.
     * @param[out] nodeInfo result node information.
     * @return Error.
     */
    Error GetNodeInfo(const String& nodeID, UnitNodeInfo& nodeInfo) const override;

    /**
     * Subscribes node info notifications.
     *
     * @param listener node info listener.
     * @return Error.
     */
    Error SubscribeListener(nodeinfoprovider::NodeInfoListenerItf& listener) override;

    /**
     * Unsubscribes from node info notifications.
     *
     * @param listener node info listener.
     * @return Error.
     */
    Error UnsubscribeListener(nodeinfoprovider::NodeInfoListenerItf& listener) override;

    /**
     * Notifies that SM is connected.
     *
     * @param nodeID SM node ID.
     */
    void OnSMConnected(const String& nodeID) override;

    /**
     * Notifies that SM is disconnected.
     *
     * @param nodeID SM node ID.
     * @param err disconnect reason.
     */
    void OnSMDisconnected(const String& nodeID, const Error& err) override;

    /**
     * Receives SM info.
     *
     * @param info SM info.
     * @return Error.
     */
    Error OnSMInfoReceived(const SMInfo& info) override;

private:
    static constexpr auto cListenersSize = 4;
    static constexpr auto cAllocatorSize
        = sizeof(UnitNodeInfo) + sizeof(StaticArray<StaticString<cIDLen>, cMaxNumNodes>);

    void OnNodeInfoChanged(const NodeInfo& info) override;

    NodeInfoCache* AddOrGetCacheItem(const String& nodeID);
    void           NotifyListeners(const NodeInfoCache& info);
    Error          SendNotification(const NodeInfoCache& info, bool sendImmediately = false);
    Error          ScheduleNotification(const String& nodeID);
    void           Run();

    mutable Mutex                                                       mMutex;
    StaticAllocator<cAllocatorSize>                                     mAllocator;
    Thread<>                                                            mThread;
    ConditionalVariable                                                 mCondVar;
    bool                                                                mRunning {};
    Config                                                              mConfig;
    StaticArray<NodeInfoCache, cMaxNumNodes>                            mCache;
    StaticArray<StaticString<cIDLen>, cMaxNumNodes>                     mNotificationQueue;
    StaticArray<nodeinfoprovider::NodeInfoListenerItf*, cListenersSize> mListeners;
    iamclient::NodeInfoProviderItf*                                     mNodeInfoProvider {};
};

/** @}*/

} // namespace aos::cm::nodeinfoprovider

#endif
