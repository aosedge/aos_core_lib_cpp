/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_COMMUNICATION_HPP_
#define AOS_CM_COMMUNICATION_HPP_

#include "aos/common/cloudprotocol/state.hpp"
#include "aos/common/types.hpp"

namespace aos::cm::communication {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Communication interface.
 */
class CommunicationItf {
public:
    /**
     * Sends instances new state.
     *
     * @param newState new state to send.
     * @return Error.
     */
    virtual Error SendInstanceNewState(const cloudprotocol::NewState& newState) = 0;

    /**
     * Sends instances update state.
     *
     * @param updateState update state to send.
     * @return Error.
     */
    virtual Error SendInstanceStateRequest(const cloudprotocol::StateRequest& stateRequest) = 0;

    /**
     * Destructor.
     */
    virtual ~CommunicationItf() = default;
};

} // namespace aos::cm::communication

#endif
