/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_STATUSHANDLER_ITF_SENDER_HPP_
#define AOS_CORE_SM_STATUSHANDLER_ITF_SENDER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::sm::statushandler {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Sender interface.
 */
class SenderItf {
public:
    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;

    /**
     * Sends a single instance status.
     *
     * @param status instance status.
     * @return Error.
     */
    virtual Error SendInstanceStatus(const InstanceStatus& status) = 0;
};

/** @}*/

} // namespace aos::sm::statushandler

#endif
