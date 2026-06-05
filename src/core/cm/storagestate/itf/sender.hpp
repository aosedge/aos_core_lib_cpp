/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_STORAGESTATE_ITF_SENDER_HPP_
#define AOS_CORE_CM_STORAGESTATE_ITF_SENDER_HPP_

#include <core/common/types/state.hpp>

namespace aos::cm::storagestate {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Sender interface.
 */
class SenderItf {
public:
    /**
     * Sends state request for the instance.
     *
     * @param request state request.
     * @return Error.
     */
    virtual Error SendStateRequest(const StateRequest& request) = 0;

    /**
     * Sends instance's new state.
     *
     * @param state new state.
     * @return Error.
     */
    virtual Error SendNewState(const NewState& state) = 0;

    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;
};

/** @}*/

} // namespace aos::cm::storagestate

#endif
