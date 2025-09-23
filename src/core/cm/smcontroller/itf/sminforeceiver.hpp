/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_SMCONTROLLER_ITF_SMSTATUSRECEIVER_HPP_
#define AOS_CORE_CM_SMCONTROLLER_ITF_SMSTATUSRECEIVER_HPP_

#include <core/common/types/types.hpp>

namespace aos::cm::smcontroller {

/** @addtogroup cm Communication Manager
 *  @{
 */

struct SMInfo {
    StaticString<cNodeIDLen> mNodeID;
    ResourceInfoArray        mResources;
    RuntimeInfoArray         mRuntimes;

    /**
     * Compares SM status.
     *
     * @param status SM status to compare with.
     * @return bool.
     */
    bool operator==(const SMInfo& status) const
    {
        return mNodeID == status.mNodeID && mResources == status.mResources && mRuntimes == status.mRuntimes;
    }

    /**
     * Compares SM info.
     *
     * @param status SM info to compare with.
     * @return bool.
     */
    bool operator!=(const SMInfo& status) const { return !operator==(status); }
};

/**
 * Receives SM info updates.
 */
class SMInfoReceiverItf {
public:
    /**
     * Destructor.
     */
    virtual ~SMInfoReceiverItf() = default;

    /**
     * Notifies that SM is connected.
     *
     * @param nodeID SM node ID.
     */
    virtual void OnSMConnected(const String& nodeID) = 0;

    /**
     * Notifies that SM is disconnected.
     *
     * @param nodeID SM node ID.
     * @param err disconnect reason.
     */
    virtual void OnSMDisconnected(const String& nodeID, const Error& err) = 0;

    /**
     * Receives SM info.
     *
     * @param info SM info.
     * @return Error.
     */
    virtual Error OnSMInfoReceived(const SMInfo& info) = 0;
};

} // namespace aos::cm::smcontroller

#endif
