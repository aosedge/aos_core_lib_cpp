/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_COMMUNICATION_HPP_
#define AOS_CM_COMMUNICATION_HPP_

#include "aos/common/cloudprotocol/cloudmessage.hpp"
#include "aos/common/types.hpp"

namespace aos::cm::communication {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Message handler interface to handle received message.
 */
class MessageHandlerItf {
public:
    /**
     * Handles received message.
     *
     * @param message received message.
     * @return Error.
     */
    virtual Error HandleMessage(const cloudprotocol::MessageVariant& message) = 0;

    /**
     * Destructor.
     */
    virtual ~MessageHandlerItf() = default;
};

/**
 * Communication interface.
 */
class CommunicationItf {
public:
    /**
     * Sends cloud message.
     *
     * @param body cloud message body to send.
     * @return Error.
     */
    virtual Error SendMessage(const cloudprotocol::MessageVariant& body) = 0;

    /**
     * Destructor.
     */
    virtual ~CommunicationItf() = default;
};

} // namespace aos::cm::communication

#endif
