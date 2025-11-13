/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_SM_LAUNCHER_ITF_INSTANCESTATUSRECEIVER_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_INSTANCESTATUSRECEIVER_HPP_

#include <core/common/types/common.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Instance status.
 */
struct InstanceStatus {
    InstanceIdent        mIdent;
    StaticString<cIDLen> mRuntimeID;
    InstanceState        mState;
    Error                mError;

    /**
     * Compares instance status.
     *
     * @param rhs instance status to compare.
     * @return bool.
     */
    bool operator==(const InstanceStatus& rhs) const
    {
        return mIdent == rhs.mIdent && mRuntimeID == rhs.mRuntimeID && mState == rhs.mState && mError == rhs.mError;
    }

    /**
     * Compares instance status.
     *
     * @param rhs instance status to compare.
     * @return bool.
     */
    bool operator!=(const InstanceStatus& rhs) const { return !operator==(rhs); }
};

/**
 * Instance status receiver interface.
 */
class InstanceStatusReceiverItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceStatusReceiverItf() = default;

    /**
     * Notifies about instance status change.
     *
     * @param instanceStatus instance status.
     */
    virtual void OnInstanceStatusChanged(const InstanceStatus& status) = 0;
};

} // namespace aos::sm::launcher

#endif
